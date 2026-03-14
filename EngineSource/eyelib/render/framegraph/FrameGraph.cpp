#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/GLDrawDebugLabel.h"
#include <algorithm>
#include <queue>

namespace eyelib {
namespace render {

namespace {
    bool IsDepthFormat(rhi::RHITextureFormat format) {
        return format == rhi::RHITextureFormat::Depth24 || format == rhi::RHITextureFormat::Depth24Stencil8;
    }
}

rhi::ResourceState FrameGraph::DesiredReadState(rhi::RHITextureFormat format) {
    return IsDepthFormat(format) ? rhi::ResourceState::DepthRead : rhi::ResourceState::ShaderResource;
}

rhi::ResourceState FrameGraph::DesiredWriteState(rhi::RHITextureFormat format) {
    return IsDepthFormat(format) ? rhi::ResourceState::DepthWrite : rhi::ResourceState::RenderTarget;
}

void FrameGraph::TransitionPassResources(size_t passIndex, rhi::ICommandList* cmd) {
    if (!cmd) return;
    const auto& p = m_passes[passIndex];
    // First transition all reads to desired read state
    for (FGTextureHandle h : p.reads) {
        if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) continue;
        auto& tex = m_textures[h];
        if (!tex.rhiTexture) continue;
        rhi::ResourceState desired = DesiredReadState(tex.desc.format);
        if (tex.currentState != desired) {
            cmd->BarrierTexture(tex.rhiTexture, tex.currentState, desired);
            tex.currentState = desired;
        }
    }
    // Then transition all writes to desired write state
    for (FGTextureHandle h : p.writes) {
        if (h < 0 || static_cast<size_t>(h) >= m_textures.size()) continue;
        auto& tex = m_textures[h];
        if (!tex.rhiTexture) continue;
        rhi::ResourceState desired = DesiredWriteState(tex.desc.format);
        if (tex.currentState != desired) {
            cmd->BarrierTexture(tex.rhiTexture, tex.currentState, desired);
            tex.currentState = desired;
        }
    }
}

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() {
    Clear();
    if (m_device) {
        for (auto& [key, list] : m_transientPool)
            for (rhi::ITexture* tex : list)
                m_device->DestroyTexture(tex);
        m_transientPool.clear();
    }
}

FGTextureHandle FrameGraph::AllocHandle() {
    return static_cast<FGTextureHandle>(m_textures.size());
}

rhi::ITexture* FrameGraph::AcquireTransient(const FGTransientKey& key) {
    if (!m_device || !key.width || !key.height) return nullptr;
    auto& list = m_transientPool[key];
    if (!list.empty()) {
        rhi::ITexture* tex = list.back();
        list.pop_back();
        return tex;
    }
    rhi::TextureDesc td;
    td.type = rhi::TextureType::Texture2D;
    td.width = key.width;
    td.height = key.height;
    td.format = key.format;
    td.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    td.mipLevels = 1;
    td.samples = 1;
    return m_device->CreateTexture(td, nullptr);
}

void FrameGraph::ReleaseTransient(rhi::ITexture* tex, const FGTransientKey& key) {
    if (!tex) return;
    m_transientPool[key].push_back(tex);
}

void FrameGraph::ResolveAliasing() {
    if (!m_device) return;
    // Group non-imported texture indices by (width, height, format)
    std::map<FGTransientKey, std::vector<size_t>> buckets;
    for (size_t i = 0; i < m_textures.size(); ++i) {
        auto& t = m_textures[i];
        if (t.imported || t.rhiTexture) continue;
        if (!t.poolKey.width) t.poolKey.width = t.width ? t.width : m_width;
        if (!t.poolKey.height) t.poolKey.height = t.height ? t.height : m_height;
        t.poolKey.format = t.desc.format;
        if (!t.poolKey.width || !t.poolKey.height) continue;
        buckets[t.poolKey].push_back(i);
    }
    // Per bucket: greedy alias slot assignment (non-overlapping lifetimes share a slot)
    for (auto& [key, indices] : buckets) {
        // Sort by firstUsePass so we assign in order
        std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
            return m_textures[a].firstUsePass < m_textures[b].firstUsePass;
        });
        std::vector<int> slotLastUse; // slotLastUse[s] = last pass index using slot s
        for (size_t idx : indices) {
            auto& t = m_textures[idx];
            int first = t.firstUsePass;
            int last = t.lastUsePass;
            int slot = -1;
            for (size_t s = 0; s < slotLastUse.size(); ++s) {
                if (slotLastUse[s] < first) {
                    slot = static_cast<int>(s);
                    break;
                }
            }
            if (slot < 0) {
                slot = static_cast<int>(slotLastUse.size());
                slotLastUse.push_back(-1);
            }
            slotLastUse[slot] = last;
            t.aliasSlot = slot;
        }
        // Acquire one texture per slot and assign to all nodes in this bucket
        const size_t numSlots = slotLastUse.size();
        std::vector<rhi::ITexture*> slotTextures(numSlots);
        for (size_t s = 0; s < numSlots; ++s)
            slotTextures[s] = AcquireTransient(key);
        for (size_t idx : indices) {
            auto& t = m_textures[idx];
            if (t.aliasSlot >= 0 && static_cast<size_t>(t.aliasSlot) < numSlots)
                t.rhiTexture = slotTextures[t.aliasSlot];
        }
    }
}

FGTextureHandle FrameGraph::CreateTexture(const FGTextureDesc& desc) {
    TextureNode n;
    n.desc = desc;
    n.width = desc.width ? desc.width : m_width;
    n.height = desc.height ? desc.height : m_height;
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
    // Release transient textures back to pool (each unique texture once; aliased nodes share ptr)
    std::vector<std::pair<rhi::ITexture*, FGTransientKey>> toRelease;
    for (auto& t : m_textures) {
        if (!t.imported && t.rhiTexture && t.poolKey.width && t.poolKey.height) {
            toRelease.push_back({ t.rhiTexture, t.poolKey });
            t.rhiTexture = nullptr;
        }
    }
    std::sort(toRelease.begin(), toRelease.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        if (a.second.width != b.second.width) return a.second.width < b.second.width;
        if (a.second.height != b.second.height) return a.second.height < b.second.height;
        return static_cast<int>(a.second.format) < static_cast<int>(b.second.format);
    });
    toRelease.erase(std::unique(toRelease.begin(), toRelease.end(),
        [](const auto& a, const auto& b) { return a.first == b.first; }), toRelease.end());
    for (const auto& p : toRelease)
        ReleaseTransient(p.first, p.second);
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
    m_executionOrder.clear();
}

void FrameGraph::Compile(rhi::IDevice* device) {
    m_device = device;
    m_passInfos.clear();
    m_passInfos.reserve(m_passes.size());
    for (auto& p : m_passes)
        p.culled = (!p.sideEffect && p.writes.empty());

    // Reset state and set dimensions/pool key for all textures; do not create RHI yet
    for (size_t i = 0; i < m_textures.size(); ++i) {
        auto& t = m_textures[i];
        t.currentState = rhi::ResourceState::Undefined;
        t.firstUsePass = -1;
        t.lastUsePass = -1;
        t.aliasSlot = -1;
        if (!t.imported) {
            t.rhiTexture = nullptr;
            t.width = t.desc.width ? t.desc.width : m_width;
            t.height = t.desc.height ? t.desc.height : m_height;
            t.poolKey = { t.width, t.height, t.desc.format };
        } else {
            t.poolKey = {};
        }
    }

    // Compute per-resource lifetime (first/last pass that uses each texture) in insertion order; BuildDAGAndComputeExecutionOrder will overwrite with execution-order positions.
    for (int passIdx = 0; passIdx < static_cast<int>(m_passes.size()); ++passIdx) {
        const auto& p = m_passes[passIdx];
        if (p.culled) continue;
        for (FGTextureHandle h : p.reads) {
            if (h >= 0 && static_cast<size_t>(h) < m_textures.size()) {
                auto& t = m_textures[h];
                if (t.firstUsePass < 0) t.firstUsePass = passIdx;
                t.lastUsePass = passIdx;
            }
        }
        for (FGTextureHandle h : p.writes) {
            if (h >= 0 && static_cast<size_t>(h) < m_textures.size()) {
                auto& t = m_textures[h];
                if (t.firstUsePass < 0) t.firstUsePass = passIdx;
                t.lastUsePass = passIdx;
            }
        }
    }

    // Build DAG from read/write deps, topological sort, recompute texture lifetimes in execution order.
    BuildDAGAndComputeExecutionOrder();

    // Resolve transients: pool + aliasing (acquire from pool, assign by lifetime)
    ResolveAliasing();

    // Imported textures keep their existing rhiTexture; restore from desc for display
    for (auto& t : m_textures) {
        if (t.imported) continue;
        if (!t.width && t.desc.width) t.width = t.desc.width;
        if (!t.height && t.desc.height) t.height = t.desc.height;
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

void FrameGraph::BuildDAGAndComputeExecutionOrder() {
    const size_t n = m_passes.size();
    m_executionOrder.clear();
    if (n == 0) return;

    // Build adjacency list: adj[i] = passes that must run after pass i (edge i -> j means i before j)
    std::vector<std::vector<size_t>> adj(n);
    for (size_t h = 0; h < m_textures.size(); ++h) {
        std::vector<size_t> writers, readers;
        for (size_t p = 0; p < n; ++p) {
            if (m_passes[p].culled) continue;
            for (FGTextureHandle w : m_passes[p].writes)
                if (w >= 0 && static_cast<size_t>(w) == h) { writers.push_back(p); break; }
            for (FGTextureHandle r : m_passes[p].reads)
                if (r >= 0 && static_cast<size_t>(r) == h) { readers.push_back(p); break; }
        }
        // Writer must run before every reader
        for (size_t w : writers)
            for (size_t r : readers)
                if (w != r) adj[w].push_back(r);
        // Multiple writers: declaration order (lower pass index first)
        std::sort(writers.begin(), writers.end());
        for (size_t i = 0; i < writers.size(); ++i)
            for (size_t j = i + 1; j < writers.size(); ++j)
                adj[writers[i]].push_back(writers[j]);
    }

    // Kahn's algorithm (topological sort)
    std::vector<int> inDegree(n, 0);
    for (size_t i = 0; i < n; ++i)
        for (size_t j : adj[i]) inDegree[j]++;
    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i)
        if (inDegree[i] == 0) q.push(i);
    while (!q.empty()) {
        size_t i = q.front();
        q.pop();
        m_executionOrder.push_back(i);
        for (size_t j : adj[i])
            if (--inDegree[j] == 0) q.push(j);
    }
    if (m_executionOrder.size() != n) {
        core::Log::Warn("FrameGraph: cycle in pass dependencies; using insertion order.");
        m_executionOrder.resize(n);
        for (size_t i = 0; i < n; ++i) m_executionOrder[i] = i;
    }

    // Execution position for each pass (0 = first to run)
    std::vector<size_t> execPos(n);
    for (size_t i = 0; i < n; ++i) execPos[m_executionOrder[i]] = i;

    // Recompute texture lifetimes in execution-order space for aliasing
    for (size_t t = 0; t < m_textures.size(); ++t) {
        int first = -1, last = -1;
        for (size_t p = 0; p < n; ++p) {
            if (m_passes[p].culled) continue;
            bool uses = false;
            for (FGTextureHandle rh : m_passes[p].reads)
                if (rh >= 0 && static_cast<size_t>(rh) == t) { uses = true; break; }
            if (!uses)
                for (FGTextureHandle wh : m_passes[p].writes)
                    if (wh >= 0 && static_cast<size_t>(wh) == t) { uses = true; break; }
            if (!uses) continue;
            size_t pos = execPos[p];
            if (first < 0 || static_cast<size_t>(first) > pos) first = static_cast<int>(pos);
            if (last < 0 || static_cast<size_t>(last) < pos) last = static_cast<int>(pos);
        }
        m_textures[t].firstUsePass = first;
        m_textures[t].lastUsePass = last;
    }
}

void FrameGraph::Execute(rhi::ICommandList* cmd) {
    FGPassResources res{
        [this](FGTextureHandle h) { return Resolve(h); },
        [this](FGTextureHandle h) { return ResolveRHI(h); },
        m_width,
        m_height
    };
    const bool useDAGOrder = !m_executionOrder.empty();
    const size_t numPasses = useDAGOrder ? m_executionOrder.size() : m_passes.size();
    for (size_t idx = 0; idx < numPasses; ++idx) {
        const size_t i = useDAGOrder ? m_executionOrder[idx] : idx;
        auto& p = m_passes[i];
        if (p.culled) continue;
        TransitionPassResources(i, cmd);
        core::SetGLDrawDebugLabel(p.name.c_str());
        const bool skipTimestamp = (p.name == "GPU Cull");  // Avoid GL 1282 with query + compute on some drivers
        if (cmd && p.queryPool && !skipTimestamp)
            cmd->BeginQuery(p.queryPool, 0);
        p.execute(res);
        if (cmd && p.queryPool && !skipTimestamp) {
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
