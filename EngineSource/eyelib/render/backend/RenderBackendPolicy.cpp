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

bool SupportsBindless(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsBindless;
}

bool SupportsCompute(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsCompute;
}

bool SupportsMultiDrawIndirect(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsMultiDrawIndirect;
}

bool SupportsMultiDrawIndirectCount(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsMultiDrawIndirectCount;
}

bool SupportsGpuDrivenPerDrawTransformIndirect(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsGpuDrivenPerDrawTransformIndirect;
}

bool SupportsResourceSpaceBindPath(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsResourceSpaceBindPath;
}

bool SupportsAtomicCounters(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsAtomicCounters;
}

bool SupportsShaderSubroutines(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsShaderSubroutines;
}

bool SupportsTransformFeedback(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsTransformFeedback;
}

bool SupportsConditionalRender(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsConditionalRender;
}

}  // namespace backend
}  // namespace render
}  // namespace eyelib

