#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/passes/detail/GpuCullPass.h"
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

/** Per-frame CPU scratch for GPU cull draw list + indirect path flags (owned by GpuCullPassSystem). */
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
 * Owns GpuCullPass, per-draw transform buffer, and CPU scratch for draw-list prep + upload.
 */
class GpuCullPassSystem {
public:
    GpuCullPassSystem() = default;
    ~GpuCullPassSystem();

    GpuCullPassSystem(const GpuCullPassSystem&) = delete;
    GpuCullPassSystem& operator=(const GpuCullPassSystem&) = delete;

    /** Creates GpuCullPass (if supported), compute resources, and transform SSBO. Safe to call again after Shutdown. */
    bool Initialize(rhi::IDevice* device);
    /** Destroys transform buffer and GpuCullPass (while device is still valid). */
    void Shutdown();

    uint32_t PrepareFromVisibleOpaque(const GpuCullScenePrepUploadParams& upload, const std::vector<Drawable>& visibleOpaque);
    uint32_t PrepareFromView(const GpuCullScenePrepUploadParams& upload, const IRenderSceneView& view);

    /** Runs GPU cull compute (indirect buffer fill). No-op if pass missing or unsupported. */
    void ExecuteGpuCull(const RenderContext& ctx);

    const GpuCullSceneScratchState& Scratch() const { return m_scratch; }

    GpuCullPass* GetPass() const { return m_gpuCullPass.get(); }
    rhi::IBuffer* GetTransformBuffer() const { return m_transformBuffer; }
    bool IsSupported(rhi::IDevice* device) const;

private:
    rhi::IDevice* m_device = nullptr;
    std::unique_ptr<GpuCullPass> m_gpuCullPass;
    rhi::IBuffer* m_transformBuffer = nullptr;
    GpuCullSceneScratchState m_scratch{};
};

}  // namespace render
}  // namespace eyelib
