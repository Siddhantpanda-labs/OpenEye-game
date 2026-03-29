#pragma once

#include "eyelib/render/passes/GpuCullScenePrep.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class GpuCullPass;
class IRenderSceneView;

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
