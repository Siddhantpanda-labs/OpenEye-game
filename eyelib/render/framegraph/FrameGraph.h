#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

struct FGTextureDesc {
    unsigned width = 0;
    unsigned height = 0;
    rhi::RHITextureFormat format = rhi::RHITextureFormat::RGBA8;
    bool srgb = true;
    std::string name;
};

using FGTextureHandle = int;  // -1 invalid

/** Key for transient texture pool: (width, height, format). */
struct FGTransientKey {
    unsigned width = 0;
    unsigned height = 0;
    rhi::RHITextureFormat format = rhi::RHITextureFormat::RGBA8;
    bool operator<(const FGTransientKey& o) const {
        if (width != o.width) return width < o.width;
        if (height != o.height) return height < o.height;
        return static_cast<int>(format) < static_cast<int>(o.format);
    }
};

struct FGPassResources {
    std::function<uint32_t(FGTextureHandle)> getTexture;
    std::function<rhi::ITexture*(FGTextureHandle)> getTextureRHI;
    unsigned width = 0;
    unsigned height = 0;
};

class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();

    FGTextureHandle CreateTexture(const FGTextureDesc& desc);
    /** Import existing RHI texture into the graph (no backend-native handle). */
    FGTextureHandle ImportTextureRHI(const std::string& name, rhi::ITexture* texture, unsigned width, unsigned height);

    /** Compile the graph. Device must be non-null (used for resource creation). */
    void Compile(rhi::IDevice* device);
    /** @deprecated Use Compile(device). */
    void Compile() { if (m_device) Compile(m_device); }

    using ExecuteFunc = std::function<void(const FGPassResources&)>;

    int AddPass(const std::string& name, const ExecuteFunc& execute,
                const std::vector<FGTextureHandle>& reads = {},
                const std::vector<FGTextureHandle>& writes = {},
                bool sideEffect = false);

    void Clear();
    /** Execute with context (uses context command list). */
    void Execute(const RenderContext& ctx) { Execute(ctx.cmd); }
    /** Pass command list for pass timing (BeginQuery/EndQuery). If null, passes run without timing. */
    void Execute(rhi::ICommandList* cmd = nullptr);

    struct PassInfo { std::string name; double gpuMs = 0.0; bool culled = false; };
    const std::vector<PassInfo>& GetPasses() const { return m_passInfos; }

    /** Backend-native texture id (e.g. GL name); 0 if handle invalid or RHI-only. Prefer ResolveRHI for backend-agnostic code. */
    uint32_t Resolve(FGTextureHandle h) const;
    rhi::ITexture* ResolveRHI(FGTextureHandle h) const;
    unsigned GetWidth() const { return m_width; }
    unsigned GetHeight() const { return m_height; }
    void SetFrameSize(unsigned w, unsigned h) { m_width = w; m_height = h; }

private:
    /** Desired ResourceState for reading a texture (ShaderResource or DepthRead for depth formats). */
    static rhi::ResourceState DesiredReadState(rhi::RHITextureFormat format);
    /** Desired ResourceState for writing a texture (RenderTarget or DepthWrite for depth formats). */
    static rhi::ResourceState DesiredWriteState(rhi::RHITextureFormat format);
    /** Insert barriers so resources used by the pass are in the correct state; update tracked state. */
    void TransitionPassResources(size_t passIndex, rhi::ICommandList* cmd);

    struct TextureNode {
        FGTextureDesc desc;
        uint32_t nativeId = 0;  // legacy imported (e.g. GL texture id)
        rhi::ITexture* rhiTexture = nullptr;
        bool imported = false;
        unsigned width = 0;
        unsigned height = 0;
        /** Tracked state for automatic barrier insertion (Phase 2). */
        rhi::ResourceState currentState = rhi::ResourceState::Undefined;
        /** First and last pass index that use this resource (read or write). Lifetime is [firstUsePass, lastUsePass]. */
        int firstUsePass = -1;
        int lastUsePass = -1;
        /** For transients: alias slot within (width, height, format) bucket; multiple nodes can share one RHI texture when lifetimes don't overlap. */
        int aliasSlot = -1;
        /** For transients: pool key so we know which bucket to release to. */
        FGTransientKey poolKey{};
    };
    struct PassNode {
        std::string name;
        ExecuteFunc execute;
        std::vector<FGTextureHandle> reads, writes;
        bool sideEffect = false;
        rhi::IQueryPool* queryPool = nullptr;
        bool culled = false;
    };

    FGTextureHandle AllocHandle();
    /** Acquire a texture from the transient pool (or create); used during Compile for non-imported textures. */
    rhi::ITexture* AcquireTransient(const FGTransientKey& key);
    /** Release a texture back to the pool (called from Clear). */
    void ReleaseTransient(rhi::ITexture* tex, const FGTransientKey& key);
    /** Compute alias slots for non-imported textures with same key; assign rhiTexture from pool per slot. */
    void ResolveAliasing();
    /** Build DAG from read/write deps, topological sort into m_executionOrder; recompute texture firstUsePass/lastUsePass in execution order. On cycle, fall back to insertion order. */
    void BuildDAGAndComputeExecutionOrder();

    rhi::IDevice* m_device = nullptr;
    std::vector<TextureNode> m_textures;
    std::vector<PassNode> m_passes;
    std::vector<PassInfo> m_passInfos;
    /** DAG-derived execution order: indices into m_passes. Empty means use insertion order (e.g. before first Compile). */
    std::vector<size_t> m_executionOrder;
    unsigned m_width = 0, m_height = 0;
    /** Transient texture pool: key -> free list. Textures are returned on Clear(), reused on next Compile(). */
    std::map<FGTransientKey, std::vector<rhi::ITexture*>> m_transientPool;
};

} // namespace render
} // namespace eyelib
