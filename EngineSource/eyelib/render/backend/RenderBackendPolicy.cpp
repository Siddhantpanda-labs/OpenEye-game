#include "eyelib/render/backend/RenderBackendPolicy.h"

namespace eyelib {
namespace render {
namespace backend {

RenderBackend ResolveBackend(const rhi::IDevice* device) {
    return device ? device->GetRenderBackend() : RenderBackend::Default;
}

bool IsVulkan(const rhi::IDevice* device) {
    return ResolveBackend(device) == RenderBackend::Vulkan;
}

bool UseTypedPushConstants(const rhi::IDevice* device) {
    return IsVulkan(device);
}

bool PreferVulkanTransparencyPath(const rhi::IDevice* device, const ShaderCache* shaderCache) {
    return IsVulkan(device) && shaderCache != nullptr;
}

CameraView AdjustCameraForBackend(const rhi::IDevice* device, const CameraView& inCamera) {
    return camera::AdjustCameraForBackend(inCamera, ResolveBackend(device));
}

}  // namespace backend
}  // namespace render
}  // namespace eyelib

