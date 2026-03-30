#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/rhi/RHI.h"
#include <array>
#include <cstdint>

namespace eyelib {
namespace render {

/**
 * Compute-based light culling: builds per-cluster light index list on the GPU.
 * Replaces CPU ClusteredLightGrid::BuildLightGrid + UploadToGPU when supported.
 * Output layout matches existing light index buffer (clusterIdx * 255 + offset = uint; 255 = end).
 */
class LightCullPass {
public:
    LightCullPass() = default;
    ~LightCullPass();

    LightCullPass(const LightCullPass&) = delete;
    LightCullPass& operator=(const LightCullPass&) = delete;

    bool IsSupported(rhi::IDevice* device) const;
    /** Create compute pipeline. Device must be non-null. */
    bool Initialize(rhi::IDevice* device);
    void Shutdown();

    /** Set input light buffers (binding 3 = point, binding 4 = spot). Call before Execute. */
    void SetPointLights(rhi::IBuffer* buffer, uint32_t count) { m_pointLightBuffer = buffer; m_pointLightCount = count; }
    void SetSpotLights(rhi::IBuffer* buffer, uint32_t count) { m_spotLightBuffer = buffer; m_spotLightCount = count; }
    /** Output: light index buffer (binding 0). Must be at least totalClusters * 255 * sizeof(uint). */
    void SetOutputBuffer(rhi::IBuffer* buffer) { m_outputBuffer = buffer; }

    /** View and projection (column-major, 16 floats each). */
    void SetViewProj(const float* view16, const float* proj16);
    /** Grid params from ClusteredLightGrid. */
    void SetParams(uint32_t tileCountX, uint32_t tileCountY, uint32_t sliceCount,
                   float logNear, float logFar, float nearPlane, float farPlane,
                   uint32_t screenWidth, uint32_t screenHeight);

    /** Dispatch compute to fill output buffer. No-op if not supported or buffers missing. */
    void Execute(const RenderContext& ctx) { Execute(ctx.cmd); }
    void Execute(rhi::ICommandList* cmd);

private:
    void EnsureResources();
    void DestroyDescriptorResources();

    /** Must match DeviceVK::kMaxFrames / PassResourceBuilder::kFrames (Vulkan frames in flight). */
    static constexpr uint32_t kDescriptorSetFrames = 3u;

    rhi::IDevice* m_device = nullptr;
    rhi::IShader* m_computeShader = nullptr;
    rhi::IPipeline* m_computePipeline = nullptr;
    std::array<rhi::IDescriptorSet*, kDescriptorSetFrames> m_frameSets{};
    std::array<rhi::IDescriptorSet*, kDescriptorSetFrames> m_lightCullPassSets{};
    std::array<rhi::IDescriptorSet*, kDescriptorSetFrames> m_sceneBuffersSets{};
    /** SceneBuffers bindings 0–1 unused by light_cull.comp but required by shared Vulkan layout; 2–3 fall back when no lights. */
    rhi::IBuffer* m_sceneFallbackBuffer = nullptr;
    std::array<rhi::IBuffer*, kDescriptorSetFrames> m_paramsUBOs{};
    rhi::IBuffer* m_pointLightBuffer = nullptr;
    rhi::IBuffer* m_spotLightBuffer = nullptr;
    rhi::IBuffer* m_outputBuffer = nullptr;
    uint32_t m_pointLightCount = 0;
    uint32_t m_spotLightCount = 0;
    float m_view[16] = {};
    float m_proj[16] = {};
    uint32_t m_tileCountX = 0, m_tileCountY = 0, m_sliceCount = 24;
    float m_logNear = 0.0f, m_logFar = 0.0f, m_nearPlane = 0.1f, m_farPlane = 1000.0f;
    uint32_t m_screenWidth = 0, m_screenHeight = 0;
    bool m_initialized = false;
    bool m_initAttempted = false;
};

} // namespace render
} // namespace eyelib
