#include "eyelib/rendering/backend/RenderBackendPolicy.h"

namespace eyelib {
namespace rendering {
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

bool SupportsAsyncCompute(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsAsyncCompute;
}

bool AllowsAsyncComputeQueueRouting(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsAsyncCompute &&
           device->GetAsyncComputeSubmitPolicy() != rhi::AsyncComputeSubmitPolicy::ForceGraphicsQueue;
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

bool SupportsPresentMailbox(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsPresentMailbox;
}

bool SupportsPresentImmediate(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsPresentImmediate;
}

bool SupportsGpuTimestampQueries(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsGpuTimestampQueries;
}

bool SupportsOcclusionQueries(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsOcclusionQueries;
}

bool SupportsHostQueryReset(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsHostQueryReset;
}

bool SupportsPipelineStatisticsQueries(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsPipelineStatisticsQueries;
}

bool SupportsSynchronization2(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsSynchronization2;
}

bool SupportsBufferDeviceAddress(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsBufferDeviceAddress;
}

bool SupportsDescriptorBuffer(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsDescriptorBuffer;
}

bool SupportsGraphicsPipelineLibrary(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsGraphicsPipelineLibrary;
}

bool SupportsSwapchainColorspace(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsSwapchainColorspace;
}

bool SupportsPresentWait(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsPresentWait;
}

bool SupportsPipelineExecutableProperties(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsPipelineExecutableProperties;
}

bool SupportsDeviceFault(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsDeviceFault;
}

uint32_t SubgroupWaveSize(const rhi::IDevice* device) {
    return device ? device->GetCapabilities().subgroupSize : 0u;
}

bool SupportsMeshShader(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsMeshShader;
}

bool SupportsVariableRateShading(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsVariableRateShading;
}

bool SupportsRayQuery(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsRayQuery;
}

bool SupportsAccelerationStructureBuild(const rhi::IDevice* device) {
    return device && device->GetCapabilities().supportsAccelerationStructureBuild;
}

}  // namespace backend
}  // namespace rendering
}  // namespace eyelib

