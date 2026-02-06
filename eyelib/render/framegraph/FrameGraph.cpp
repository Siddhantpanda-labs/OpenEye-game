#include "eyelib/render/framegraph/FrameGraph.h"
#include <glad/glad.h>
#include <algorithm>

namespace eyelib {
namespace render {

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() {
    Clear();
}

FGTextureHandle FrameGraph::AllocHandle() {
    return static_cast<FGTextureHandle>(m_textures.size());
}

void FrameGraph::CreateTextureGL(TextureNode& n) {
    unsigned w = n.width ? n.width : m_width;
    unsigned h = n.height ? n.height : m_height;
    if (!w || !h) return;
    glGenTextures(1, &n.glId);
    glBindTexture(GL_TEXTURE_2D, n.glId);
    if (n.desc.internalFormat == GL_SRGB8_ALPHA8)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    else if (n.desc.internalFormat == GL_RGBA16F)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, n.desc.internalFormat, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    n.width = w;
    n.height = h;
}

FGTextureHandle FrameGraph::CreateTexture(const FGTextureDesc& desc) {
    TextureNode n;
    n.desc = desc;
    n.width = desc.width ? desc.width : m_width;
    n.height = desc.height ? desc.height : m_height;
    m_textures.push_back(n);
    return static_cast<FGTextureHandle>(m_textures.size() - 1);
}

FGTextureHandle FrameGraph::ImportTexture(const std::string& name, GLuint glId, unsigned width, unsigned height) {
    TextureNode n;
    n.desc.name = name;
    n.glId = glId;
    n.imported = true;
    n.width = width;
    n.height = height;
    m_textures.push_back(n);
    return static_cast<FGTextureHandle>(m_textures.size() - 1);
}

FGTextureHandle FrameGraph::ImportTextureRHI(const std::string& name, rhi::ITexture* texture, unsigned width, unsigned height) {
    TextureNode n;
    n.desc.name = name;
    n.rhiTexture = texture;
    n.imported = true;
    n.width = width;
    n.height = height;
    m_textures.push_back(n);
    return static_cast<FGTextureHandle>(m_textures.size() - 1);
}

int FrameGraph::AddPass(const std::string& name, const ExecuteFunc& execute,
                        const std::vector<FGTextureHandle>& reads,
                        const std::vector<FGTextureHandle>& writes,
                        bool sideEffect) {
    PassNode p;
    p.name = name;
    p.execute = execute;
    p.reads = reads;
    p.writes = writes;
    p.sideEffect = sideEffect;
    glGenQueries(2, p.timeQuery);
    m_passes.push_back(p);
    return static_cast<int>(m_passes.size() - 1);
}

void FrameGraph::Clear() {
    for (auto& t : m_textures) {
        if (!t.imported && t.glId) {
            glDeleteTextures(1, &t.glId);
            t.glId = 0;
        }
    }
    m_textures.clear();
    for (auto& p : m_passes) {
        if (p.timeQuery[0]) glDeleteQueries(2, p.timeQuery);
        p.timeQuery[0] = p.timeQuery[1] = 0;
    }
    m_passes.clear();
    m_passInfos.clear();
}

void FrameGraph::Compile() {
    m_passInfos.clear();
    m_passInfos.reserve(m_passes.size());
    for (auto& p : m_passes)
        p.culled = (!p.sideEffect && p.writes.empty());

    for (size_t i = 0; i < m_textures.size(); ++i) {
        auto& t = m_textures[i];
        if (t.imported || t.glId) continue;
        CreateTextureGL(t);
    }

    for (const auto& p : m_passes) {
        PassInfo info;
        info.name = p.name;
        info.culled = p.culled;
        m_passInfos.push_back(info);
    }
}

void FrameGraph::Execute() {
    FGPassResources res{
        [this](FGTextureHandle h) { return Resolve(h); },
        [this](FGTextureHandle h) { return ResolveRHI(h); },
        m_width,
        m_height
    };
    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& p = m_passes[i];
        if (p.culled) continue;
        glBeginQuery(GL_TIME_ELAPSED, p.timeQuery[0]);
        p.execute(res);
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 elapsed = 0;
        glGetQueryObjectui64v(p.timeQuery[0], GL_QUERY_RESULT, &elapsed);
        m_passInfos[i].gpuMs = static_cast<double>(elapsed) / 1e6;
    }
}

GLuint FrameGraph::Resolve(FGTextureHandle h) const {
    if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) return 0;
    return m_textures[h].glId;
}

rhi::ITexture* FrameGraph::ResolveRHI(FGTextureHandle h) const {
    if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) return nullptr;
    return m_textures[h].rhiTexture;
}

} // namespace render
} // namespace eyelib
