#pragma once

#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {

class ShaderCache;

namespace backend {

/** Resolves backend from device; returns Default when device is null. */
RenderBackend ResolveBackend(const rhi::IDevice* device);
/** @return true when resolved backend is Vulkan. */
bool IsVulkan(const rhi::IDevice* device);
/** @return true when typed push-constant path should be used. */
bool UseTypedPushConstants(const rhi::IDevice* device);
/**
 * Chooses transparency path policy based on backend capability and shader cache availability.
 * @param device Active render device.
 * @param shaderCache Shader cache used by Vulkan transparency path.
 * @return true when Vulkan transparency path is preferred.
 */
bool PreferVulkanTransparencyPath(const rhi::IDevice* device, const ShaderCache* shaderCache);
/**
 * Applies backend camera convention adjustment.
 * @param device Active render device.
 * @param inCamera Camera in canonical engine convention.
 * @return Camera adjusted for active backend convention.
 */
CameraView AdjustCameraForBackend(const rhi::IDevice* device, const CameraView& inCamera);
/** @return true when device exposes bindless heap / descriptor indexing path. */
bool SupportsBindless(const rhi::IDevice* device);
/** @return true when device can run compute shaders (dispatch). */
bool SupportsCompute(const rhi::IDevice* device);
/** @return true when device exposes a dedicated async compute queue path. */
bool SupportsAsyncCompute(const rhi::IDevice* device);
/** @return true when async compute is supported and the device is not in `AsyncComputeSubmitPolicy::ForceGraphicsQueue` (Vulkan deferred submits may use the compute queue). */
bool AllowsAsyncComputeQueueRouting(const rhi::IDevice* device);
/** @return true when device supports multi-draw indirect (MDI). */
bool SupportsMultiDrawIndirect(const rhi::IDevice* device);
/** @return true when device supports MDI with GPU-written draw count (indirect parameters). */
bool SupportsMultiDrawIndirectCount(const rhi::IDevice* device);
/** @return true when GPU-driven GBuffer indirect path can fetch per-draw transforms in vertex stage. */
bool SupportsGpuDrivenPerDrawTransformIndirect(const rhi::IDevice* device);
/** @return true when resource-space / descriptor-set bind path is valid (layout + BindDescriptorSets). */
bool SupportsResourceSpaceBindPath(const rhi::IDevice* device);
/** @return true when atomic counter buffers are exposed to shaders. */
bool SupportsAtomicCounters(const rhi::IDevice* device);
/** @return true when shader subroutines are used for uniform subroutine selection. */
bool SupportsShaderSubroutines(const rhi::IDevice* device);
/** @return true when transform feedback capture is supported. */
bool SupportsTransformFeedback(const rhi::IDevice* device);
/** @return true when conditional render (predicate queries) is supported. */
bool SupportsConditionalRender(const rhi::IDevice* device);
/** @return true when swapchain surface supports mailbox present (Vulkan). */
bool SupportsPresentMailbox(const rhi::IDevice* device);
/** @return true when swapchain surface supports immediate present (Vulkan). */
bool SupportsPresentImmediate(const rhi::IDevice* device);
/** @return true when GPU timestamp queries are supported (`timestampPeriod` > 0). */
bool SupportsGpuTimestampQueries(const rhi::IDevice* device);
/** @return true when occlusion query pools are supported. */
bool SupportsOcclusionQueries(const rhi::IDevice* device);
/** @return true when host `vkResetQueryPool` is available (`IDevice::ResetQueryPoolOnHost`). */
bool SupportsHostQueryReset(const rhi::IDevice* device);
/** @return true when pipeline statistics query pools are supported. */
bool SupportsPipelineStatisticsQueries(const rhi::IDevice* device);
/** @return true when Vulkan synchronization2 barriers are enabled (`vkCmdPipelineBarrier2`). */
bool SupportsSynchronization2(const rhi::IDevice* device);
/** @return true when Vulkan buffer device address is enabled (`vkGetBufferDeviceAddress`). */
bool SupportsBufferDeviceAddress(const rhi::IDevice* device);
/** @return true when `VK_EXT_descriptor_buffer` capability is enabled. */
bool SupportsDescriptorBuffer(const rhi::IDevice* device);
/** @return true when `VK_EXT_graphics_pipeline_library` capability is enabled. */
bool SupportsGraphicsPipelineLibrary(const rhi::IDevice* device);
/** @return true when `VK_EXT_swapchain_colorspace` is enabled (HDR / wide-gamut surface formats). */
bool SupportsSwapchainColorspace(const rhi::IDevice* device);
/** @return true when `VK_KHR_present_wait` is enabled (`IDevice::WaitForPresent`). */
bool SupportsPresentWait(const rhi::IDevice* device);
/** @return true when `VK_KHR_pipeline_executable_properties` was enabled at device create (`RHICapabilities::supportsPipelineExecutableProperties`). */
bool SupportsPipelineExecutableProperties(const rhi::IDevice* device);
/** @return true when `VK_EXT_device_fault` was enabled at device create (`RHICapabilities::supportsDeviceFault`). */
bool SupportsDeviceFault(const rhi::IDevice* device);
/** @return `RHICapabilities::subgroupSize` from `vkGetPhysicalDeviceProperties2` (0 if unknown / no device). */
uint32_t SubgroupWaveSize(const rhi::IDevice* device);
/** @return true when Vulkan mesh shading (`VK_EXT_mesh_shader`) is enabled (`RHICapabilities::supportsMeshShader`). */
bool SupportsMeshShader(const rhi::IDevice* device);
/** @return true when Vulkan variable rate shading (`VK_KHR_fragment_shading_rate`) is enabled. */
bool SupportsVariableRateShading(const rhi::IDevice* device);
/** @return true when Vulkan ray query + acceleration-structure slice is enabled. */
bool SupportsRayQuery(const rhi::IDevice* device);
/** @return true when `vkCmdBuildAccelerationStructuresKHR` is available (`RHICapabilities::supportsAccelerationStructureBuild`). */
bool SupportsAccelerationStructureBuild(const rhi::IDevice* device);

}  // namespace backend
}  // namespace rendering
}  // namespace eyelib

