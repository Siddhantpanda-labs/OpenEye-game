#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace render {

/**
 * Per-draw entry for GPU cull input. Layout must match cull_indirect_with_drawlist.comp DrawEntry.
 * AABB in world space; compute derives frustum from view-proj and culls.
 * Shader uses scalars (not vec3) for AABB to avoid std430 vec3 alignment ambiguity; 52 bytes.
 */
struct GpuCullDrawEntry {
    float aabbMin[3];
    float _pad0;
    float aabbMax[3];
    float _pad1;
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t baseVertex;
    uint32_t baseInstance;
};
static_assert(sizeof(GpuCullDrawEntry) == 52, "GpuCullDrawEntry layout must match shader DrawEntry (52 bytes)");

/**
 * GPU culling pass: runs a compute shader that writes DrawIndexedIndirectCommand(s)
 * into a buffer. Use with MultiDrawIndexedIndirect in a subsequent pass.
 * E4 / A1: Two variants — with draw list (frustum cull) or no draw list (single quad).
 */
class GpuCullPass {
public:
    GpuCullPass() = default;
    ~GpuCullPass();

    GpuCullPass(const GpuCullPass&) = delete;
    GpuCullPass& operator=(const GpuCullPass&) = delete;

    /** Check if device supports compute and multi-draw indirect. */
    bool IsSupported(rhi::IDevice* device) const;
    /** Create indirect buffer and compute pipeline. Device must be non-null. */
    bool Initialize(rhi::IDevice* device);
    void Shutdown();

    /** Set draw list for frustum culling (A1). Data is uploaded into the pass's combined buffer. Call before Execute. */
    void SetDrawList(const GpuCullDrawEntry* entries, uint32_t numDraws);
    /** Set view-projection matrix (column-major, 16 floats). Required when using draw list. */
    void SetViewProj(const float* viewProj16);
    /** Optional: depth texture from previous frame for occlusion culling. Same view-proj as current. Pass nullptr to disable. */
    void SetOcclusionDepth(rhi::ITexture* depthTexture, uint32_t width, uint32_t height);

    /** Run compute to fill indirect buffer (context-based). */
    void Execute(const RenderContext& ctx) { Execute(ctx.cmd); }
    /** Run compute to fill indirect buffer; inserts barrier for IndirectDraw. No-op if not supported. */
    void Execute(rhi::ICommandList* cmd);

    rhi::IBuffer* GetIndirectBuffer() const { return m_indirectBuffer; }
    /** Number of draw commands to pass to MultiDrawIndexedIndirect (clamped to indirect buffer size). */
    uint32_t GetDrawCount() const { return m_effectiveDrawCount; }
    /** Stride between DrawIndexedIndirectCommand entries (sizeof(DrawIndexedIndirectCommand)). */
    uint32_t GetIndirectStride() const { return static_cast<uint32_t>(sizeof(rhi::DrawIndexedIndirectCommand)); }

private:
    void EnsureResources();

    rhi::IDevice* m_device = nullptr;
    /** Combined buffer: [0, kIndirectRegionSize) = indirect commands (compute output), [kIndirectRegionSize, end) = draw list (CPU upload). Bound at SSBO slots 0 and 1. */
    rhi::IBuffer* m_indirectBuffer = nullptr;
    rhi::IShader* m_computeShaderWithDrawList = nullptr;
    rhi::IPipeline* m_computePipelineWithDrawList = nullptr;
    rhi::IShader* m_computeShaderNoDrawList = nullptr;
    rhi::IPipeline* m_computePipelineNoDrawList = nullptr;
    rhi::IBuffer* m_viewProjUBO = nullptr;  // View-projection for compute (UBO for pipeline compatibility)
    uint32_t m_drawCount = 1;
    uint32_t m_effectiveDrawCount = 1;  // actual count written (clamped to kMaxIndirectDraws)
    float m_viewProj[16] = {};
    rhi::ITexture* m_occlusionDepthTex = nullptr;
    uint32_t m_occlusionDepthWidth = 0;
    uint32_t m_occlusionDepthHeight = 0;
    bool m_initialized = false;
    bool m_initAttempted = false;
};

} // namespace render
} // namespace eyelib
