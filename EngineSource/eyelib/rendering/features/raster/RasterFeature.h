#pragma once

#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"

namespace eyelib {
namespace rendering {

namespace rhi {
struct IDevice;
struct IPipeline;
}

class FrameGraph;
class FrameGraphBlackboard;

/**
 * Bootstrap raster path: HDR clear → optional fullscreen compose (GPU) → blit to swapchain.
 */
class RasterFeature final : public IRenderFeature {
public:
    RasterFeature() = default;
    ~RasterFeature() override;

    const char* FeatureDebugName() const noexcept override { return writer_names::kRasterFeature; }

    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

private:
    void releaseComposePipeline() noexcept;
    void ensureComposePipeline(rhi::IDevice* device);
    static rhi::VertexLayout fullscreenVertexLayout();

    rhi::IDevice* m_deviceForPipeline = nullptr;
    rhi::IPipeline* m_composePipeline = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
