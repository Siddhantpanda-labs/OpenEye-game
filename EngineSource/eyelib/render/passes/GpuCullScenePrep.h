#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class IRenderSceneView;
class Mesh;

/** Max draws fed into GpuCullPass draw list (matches shader / buffer budget). */
inline constexpr uint32_t kMaxGpuCullDrawEntries = 256u;

/** Per-frame CPU scratch for GPU cull draw list + indirect path flags (owned by RenderPipeline). */
struct GpuCullSceneScratchState {
    std::vector<GpuCullDrawEntry> drawList;
    std::vector<math::Mat4> transforms;
    bool singleMesh = true;
    std::shared_ptr<Mesh> representativeMesh;
};

/** RHI + frame inputs for uploading scratch to GpuCullPass after CPU build. */
struct GpuCullScenePrepUploadParams {
    GpuCullPass* gpuCullPass = nullptr;
    rhi::IDevice* device = nullptr;
    rhi::IBuffer* transformBuffer = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool havePrevDepth = false;
    rhi::IRenderTarget* prevFrameDepthRT = nullptr;
    CameraView cam{};
};

/**
 * Append one drawable's world AABB + transform to GPU cull scratch lists; updates single-mesh tracking.
 * Skips invalid mesh, invalid AABB, or when draw list is full.
 */
void AppendDrawableToGpuCullScratch(const Drawable& d, uint32_t maxDraws,
    std::vector<GpuCullDrawEntry>& drawList, std::vector<math::Mat4>& transforms, Mesh*& firstMeshPtr,
    bool& singleMesh, std::shared_ptr<Mesh>& representativeMesh);

/** If no draws were produced, clear single-mesh indirect path flag. */
void FinalizeGpuCullScratchSingleMeshFlag(const std::vector<GpuCullDrawEntry>& drawList, bool& singleMesh);

/**
 * Upload CPU-built draw list + transforms, view-proj, and optional occlusion depth to GpuCullPass when supported.
 */
void ConfigureGpuCullPassFromScratch(GpuCullPass* gpuCullPass, rhi::IDevice* device, rhi::IBuffer* transformBuffer,
    uint32_t width, uint32_t height, bool havePrevDepth, rhi::IRenderTarget* prevFrameDepthRT, const CameraView& cam,
    const std::vector<GpuCullDrawEntry>& drawList, const std::vector<math::Mat4>& transforms);

/** Build scratch from `visibleOpaque` and configure GpuCullPass. Returns draw count. */
uint32_t RunGpuCullScenePrepFromDrawables(GpuCullSceneScratchState& scratch, const GpuCullScenePrepUploadParams& upload,
    const std::vector<Drawable>& drawables);

/** Build scratch by iterating scene view opaques and configure GpuCullPass. Returns draw count. */
uint32_t RunGpuCullScenePrepFromView(GpuCullSceneScratchState& scratch, const GpuCullScenePrepUploadParams& upload,
    const IRenderSceneView& view);

}  // namespace render
}  // namespace eyelib
