#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace render {

/**
 * Per-draw entry for GPU cull input. Layout must match cull_indirect.comp DrawEntry.
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
 * E4 / A1: Optional draw list input + frustum cull; fallback: one fixed draw (legacy).
 */
class GpuCullPass {
public:
    GpuCullPass() = default;
    ~GpuCullPass();

    GpuCullPass(const GpuCullPass&) = delete;
    GpuCullPass& operator=(const GpuCullPass&) = delete;

    void SetDevice(rhi::IDevice* device) { m_device = device; }

    /** Create indirect buffer and compute pipeline if caps support it. Call after SetDevice. */
    bool Initialize();
    void Shutdown();

    /** Set draw list for frustum culling (A1). Data is uploaded into the pass's combined buffer. Call before Execute. */
    void SetDrawList(const GpuCullDrawEntry* entries, uint32_t numDraws);
    /** Set view-projection matrix (column-major, 16 floats). Required when using draw list. */
    void SetViewProj(const float* viewProj16);
    /** Optional: depth texture from previous frame for occlusion culling. Same view-proj as current. Pass nullptr to disable. */
    void SetOcclusionDepth(rhi::ITexture* depthTexture, uint32_t width, uint32_t height);

    /** Run compute to fill indirect buffer; inserts barrier for IndirectDraw. No-op if not supported. */
    void Execute(rhi::ICommandList* cmd);

    rhi::IBuffer* GetIndirectBuffer() const { return m_indirectBuffer; }
    /** Number of draw commands to pass to MultiDrawIndexedIndirect (clamped to indirect buffer size). */
    uint32_t GetDrawCount() const { return m_effectiveDrawCount; }
    /** Stride between DrawIndexedIndirectCommand entries (sizeof(DrawIndexedIndirectCommand)). */
    uint32_t GetIndirectStride() const { return static_cast<uint32_t>(sizeof(rhi::DrawIndexedIndirectCommand)); }

    bool IsSupported() const;

private:
    void EnsureResources();

    rhi::IDevice* m_device = nullptr;
    /** Combined buffer: [0, kIndirectRegionSize) = indirect commands (compute output), [kIndirectRegionSize, end) = draw list (CPU upload). Single buffer bound at slot 0 and 1 to avoid GL 1282 on some drivers. */
    rhi::IBuffer* m_indirectBuffer = nullptr;
    rhi::IShader* m_computeShader = nullptr;
    rhi::IPipeline* m_computePipeline = nullptr;
    rhi::IBuffer* m_viewProjUBO = nullptr;  // UBO for u_ViewProj (avoids GL 1282 from SetUniformMat4 on compute)
    uint32_t m_drawCount = 1;
    uint32_t m_effectiveDrawCount = 1;  // actual count written (clamped to kMaxIndirectDraws)
    float m_viewProj[16] = {};
    rhi::ITexture* m_occlusionDepthTex = nullptr;
    uint32_t m_occlusionDepthWidth = 0;
    uint32_t m_occlusionDepthHeight = 0;
    bool m_initialized = false;
};

} // namespace render
} // namespace eyelib
