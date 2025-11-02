#include "FrameGraph.h"
#include "../resources/Texture.h"
#include "../../core/log/Log.h"
#include <algorithm>

namespace OpenEye { namespace Rendering {

FrameGraph::FrameGraph() = default;
FrameGraph::~FrameGraph() = default;

FGTextureHandle FrameGraph::allocHandle() { return static_cast<FGTextureHandle>(m_textures.size()); }

FGTextureHandle FrameGraph::createTexture(const FGTextureDesc& desc) {
    TextureNode node; node.desc = desc; node.width = desc.width ? desc.width : m_width; node.height = desc.height ? desc.height : m_height;
    m_textures.push_back(node);
    return static_cast<FGTextureHandle>(m_textures.size() - 1);
}

FGTextureHandle FrameGraph::importTexture(const std::string& name, GLuint glId, unsigned width, unsigned height) {
    TextureNode node; node.desc.name = name; node.glId = glId; node.imported = true; node.width = width; node.height = height;
    m_textures.push_back(node);
    return static_cast<FGTextureHandle>(m_textures.size() - 1);
}

int FrameGraph::addPass(const std::string& name, const ExecuteFunc& execute,
                        const std::vector<FGTextureHandle>& reads,
                        const std::vector<FGTextureHandle>& writes,
                        bool sideEffect) {
    PassNode p; p.name = name; p.execute = execute; p.reads = reads; p.writes = writes; p.sideEffect = sideEffect;
    glGenQueries(2, p.timeQuery);
    m_passes.push_back(p);
    return static_cast<int>(m_passes.size() - 1);
}

void FrameGraph::clear() {
    for (auto& p : m_passes) { if (p.timeQuery[0]) glDeleteQueries(2, p.timeQuery); }
    m_passes.clear(); m_passInfos.clear();
}

void FrameGraph::compile() {
    m_passInfos.clear(); m_passInfos.reserve(m_passes.size());
    // Simple culling: if pass has no writes and no side effect, cull
    for (auto& p : m_passes) {
        p.culled = (!p.sideEffect && p.writes.empty());
    }
    // Create textures for non-imported nodes
    for (size_t i = 0; i < m_textures.size(); ++i) {
        auto& t = m_textures[i];
        if (t.imported || t.glId != 0) continue;
        TextureSpec spec; spec.width = t.width ? t.width : m_width; spec.height = t.height ? t.height : m_height;
        spec.format = (t.desc.internalFormat == GL_SRGB8_ALPHA8) ? TextureFormat::RGBA8 : TextureFormat::RGBA16F; // simple mapping
        spec.isSRGB = t.desc.srgb;
        spec.generateMipmaps = false;
        Texture tex; tex.Create(spec, nullptr);
        t.glId = tex.GetID(); // lifetime not owned by Texture instance after move; in real FG we’d own wrapper
    }
    for (auto& p : m_passes) { PassInfo info; info.name = p.name; info.culled = p.culled; m_passInfos.push_back(info); }
}

void FrameGraph::execute() {
    FGPassResources res{
        [this](FGTextureHandle h) -> GLuint { return resolve(h); }, m_width, m_height
    };
    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& p = m_passes[i]; if (p.culled) continue;
        glBeginQuery(GL_TIME_ELAPSED, p.timeQuery[0]);
        p.execute(res);
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 elapsed = 0; glGetQueryObjectui64v(p.timeQuery[0], GL_QUERY_RESULT, &elapsed);
        m_passInfos[i].gpuMs = static_cast<double>(elapsed) / 1e6;
    }
}

GLuint FrameGraph::resolve(FGTextureHandle h) const {
    if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) return 0;
    return m_textures[h].glId;
}

} }


