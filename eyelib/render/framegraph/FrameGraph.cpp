#include "eyelib/render/framegraph/FrameGraph.h"
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

void FrameGraph::CreateTextureRHI(TextureNode& n) {
    if (!m_device) return;
    unsigned w = n.width ? n.width : m_width;
    unsigned h = n.height ? n.height : m_height;
    if (!w || !h) return;
    rhi::TextureDesc td;
    td.type = rhi::TextureType::Texture2D;
    td.width = w;
    td.height = h;
    td.format = n.desc.format;
    td.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    td.mipLevels = 1;
    td.samples = 1;
    n.rhiTexture = m_device->CreateTexture(td, nullptr);
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

FGTextureHandle FrameGraph::ImportTexture(const std::string& name, uint32_t nativeTextureId, unsigned width, unsigned height) {
    TextureNode n;
    n.desc.name = name;
    n.nativeId = nativeTextureId;
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
    m_passes.push_back(p);
    return static_cast<int>(m_passes.size() - 1);
}

void FrameGraph::Clear() {
    for (auto& t : m_textures) {
        if (!t.imported && t.rhiTexture && m_device) {
            m_device->DestroyTexture(t.rhiTexture);
            t.rhiTexture = nullptr;
        }
    }
    m_textures.clear();
    if (m_device) {
        for (auto& p : m_passes) {
            if (p.queryPool) {
                m_device->DestroyQueryPool(p.queryPool);
                p.queryPool = nullptr;
            }
        }
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
        if (t.imported || t.rhiTexture) continue;
        CreateTextureRHI(t);
    }

    if (m_device) {
        for (auto& p : m_passes) {
            if (!p.queryPool)
                p.queryPool = m_device->CreateQueryPool(rhi::QueryType::Timestamp, 1);
        }
    }

    for (const auto& p : m_passes) {
        PassInfo info;
        info.name = p.name;
        info.culled = p.culled;
        m_passInfos.push_back(info);
    }
}

void FrameGraph::Execute(rhi::ICommandList* cmd) {
    FGPassResources res{
        [this](FGTextureHandle h) { return Resolve(h); },
        [this](FGTextureHandle h) { return ResolveRHI(h); },
        m_width,
        m_height
    };
    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& p = m_passes[i];
        if (p.culled) continue;
        if (cmd && p.queryPool)
            cmd->BeginQuery(p.queryPool, 0);
        p.execute(res);
        if (cmd && p.queryPool) {
            cmd->EndQuery(p.queryPool, 0);
            uint64_t elapsedNs = p.queryPool->GetResult(0);
            m_passInfos[i].gpuMs = static_cast<double>(elapsedNs) / 1e6;
        }
    }
}

uint32_t FrameGraph::Resolve(FGTextureHandle h) const {
    if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) return 0;
    const auto& t = m_textures[h];
    if (t.rhiTexture) {
        void* native = t.rhiTexture->GetNativeTexture();
        return native ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(native)) : 0;
    }
    return t.nativeId;
}

rhi::ITexture* FrameGraph::ResolveRHI(FGTextureHandle h) const {
    if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) return nullptr;
    return m_textures[h].rhiTexture;
}

} // namespace render
} // namespace eyelib
