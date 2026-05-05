#pragma once

#include "eyelib/render/CameraConventions.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

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

}  // namespace backend
}  // namespace render
}  // namespace eyelib

