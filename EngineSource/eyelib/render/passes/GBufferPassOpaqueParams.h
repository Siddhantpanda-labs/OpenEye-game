#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace eyelib {
namespace render {

class GBuffer;
class GpuCullPass;
class Mesh;

/** Inputs for deferred opaque geometry submission to the GBuffer (CPU batching or GPU indirect). */
struct GBufferPassOpaqueParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    GBuffer* gbuffer = nullptr;
    std::function<void()> ensureCurrentFrameResourceSets;
    std::function<rhi::IDescriptorSet*()> acquireMaterialDrawSet;
    std::function<rhi::IDescriptorSet*()> acquireSceneDrawSet;
    rhi::IBuffer* frameUbo = nullptr;
    math::Mat4 prevView{};
    math::Mat4 prevProjection{};
    bool useGpuDrivenGBuffer = false;
    GpuCullPass* gpuCullPass = nullptr;
    Mesh* gpuDrivenQuad = nullptr;
    bool gpuCullSingleMesh = false;
    std::shared_ptr<Mesh> gpuCullRepresentativeMesh;
    rhi::IBuffer* gpuCullTransformBuffer = nullptr;
    rhi::IBuffer* instanceMatrixBuffer = nullptr;
    const RenderFrameData* frameData = nullptr;
    CameraView cam{};
    uint32_t gpuCullDrawListCount = 0;
};

}  // namespace render
}  // namespace eyelib
