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

}  // namespace backend
}  // namespace render
}  // namespace eyelib

