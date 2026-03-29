#include "eyelib/render/passes/GpuCullScenePrep.h"
#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace render {

namespace {

void ResetGpuCullScratch(GpuCullSceneScratchState& s) {
    s.drawList.clear();
    s.transforms.clear();
    s.singleMesh = true;
    s.representativeMesh.reset();
}

}  // namespace

void AppendDrawableToGpuCullScratch(const Drawable& d, uint32_t maxDraws,
    std::vector<GpuCullDrawEntry>& drawList, std::vector<math::Mat4>& transforms, Mesh*& firstMeshPtr,
    bool& singleMesh, std::shared_ptr<Mesh>& representativeMesh) {
    if (!d.mesh || !d.mesh->IsValid() || drawList.size() >= maxDraws) return;
    if (!firstMeshPtr) {
        firstMeshPtr = d.mesh.get();
        representativeMesh = d.mesh;
    } else if (d.mesh.get() != firstMeshPtr) {
        singleMesh = false;
    }
    math::AABB localAABB = d.mesh->GetAABB();
    if (!localAABB.IsValid()) return;
    math::AABB worldAABB = localAABB.Transform(d.transform);
    GpuCullDrawEntry e = {};
    e.aabbMin[0] = worldAABB.min.x; e.aabbMin[1] = worldAABB.min.y; e.aabbMin[2] = worldAABB.min.z;
    e.aabbMax[0] = worldAABB.max.x; e.aabbMax[1] = worldAABB.max.y; e.aabbMax[2] = worldAABB.max.z;
    const size_t indexCount = d.mesh->GetIndexCount();
    e.indexCount = static_cast<uint32_t>(indexCount);
    e.instanceCount = 1u;
    e.firstIndex = 0u;
    e.baseVertex = 0;
    e.baseInstance = 0u;
    drawList.push_back(e);
    transforms.push_back(d.transform);
}

void FinalizeGpuCullScratchSingleMeshFlag(const std::vector<GpuCullDrawEntry>& drawList, bool& singleMesh) {
    if (drawList.empty()) singleMesh = false;
}

void ConfigureGpuCullPassFromScratch(GpuCullPass* gpuCullPass, rhi::IDevice* device, rhi::IBuffer* transformBuffer,
    uint32_t width, uint32_t height, bool havePrevDepth, rhi::IRenderTarget* prevFrameDepthRT, const CameraView& cam,
    const std::vector<GpuCullDrawEntry>& drawList, const std::vector<math::Mat4>& transforms) {
    if (!gpuCullPass || !gpuCullPass->IsSupported(device)) return;
    const uint32_t gpuCullDrawListCount = static_cast<uint32_t>(drawList.size());
    if (gpuCullDrawListCount > 0) {
        gpuCullPass->SetDrawList(drawList.data(), gpuCullDrawListCount);
        if (transformBuffer && transforms.size() >= gpuCullDrawListCount) {
            const size_t transformSize = gpuCullDrawListCount * sizeof(math::Mat4);
            transformBuffer->Update(transforms.data(), 0, transformSize);
        }
        const math::Mat4 viewProj = cam.projection * cam.view;
        gpuCullPass->SetViewProj(&viewProj[0][0]);
    } else {
        gpuCullPass->SetDrawList(nullptr, 0);
    }
    if (havePrevDepth && prevFrameDepthRT && prevFrameDepthRT->GetDepthTexture())
        gpuCullPass->SetOcclusionDepth(prevFrameDepthRT->GetDepthTexture(), width, height);
    else
        gpuCullPass->SetOcclusionDepth(nullptr, 0, 0);
}

uint32_t RunGpuCullScenePrepFromDrawables(GpuCullSceneScratchState& scratch, const GpuCullScenePrepUploadParams& upload,
    const std::vector<Drawable>& drawables) {
    ResetGpuCullScratch(scratch);
    Mesh* firstMeshPtr = nullptr;
    for (const Drawable& d : drawables) {
        AppendDrawableToGpuCullScratch(d, kMaxGpuCullDrawEntries, scratch.drawList, scratch.transforms, firstMeshPtr,
            scratch.singleMesh, scratch.representativeMesh);
    }
    FinalizeGpuCullScratchSingleMeshFlag(scratch.drawList, scratch.singleMesh);
    const uint32_t count = static_cast<uint32_t>(scratch.drawList.size());
    ConfigureGpuCullPassFromScratch(upload.gpuCullPass, upload.device, upload.transformBuffer, upload.width, upload.height,
        upload.havePrevDepth, upload.prevFrameDepthRT, upload.cam, scratch.drawList, scratch.transforms);
    return count;
}

uint32_t RunGpuCullScenePrepFromView(GpuCullSceneScratchState& scratch, const GpuCullScenePrepUploadParams& upload,
    const IRenderSceneView& view) {
    ResetGpuCullScratch(scratch);
    Mesh* firstMeshPtr = nullptr;
    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        AppendDrawableToGpuCullScratch(d, kMaxGpuCullDrawEntries, scratch.drawList, scratch.transforms, firstMeshPtr,
            scratch.singleMesh, scratch.representativeMesh);
    });
    FinalizeGpuCullScratchSingleMeshFlag(scratch.drawList, scratch.singleMesh);
    const uint32_t count = static_cast<uint32_t>(scratch.drawList.size());
    ConfigureGpuCullPassFromScratch(upload.gpuCullPass, upload.device, upload.transformBuffer, upload.width, upload.height,
        upload.havePrevDepth, upload.prevFrameDepthRT, upload.cam, scratch.drawList, scratch.transforms);
    return count;
}

}  // namespace render
}  // namespace eyelib
