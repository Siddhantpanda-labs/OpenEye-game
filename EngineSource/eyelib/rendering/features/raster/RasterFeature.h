#pragma once

#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/rendering/shaders/ShaderLibrary.h"

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;

namespace rhi {
struct IDevice;
struct IPipeline;
}

class FrameGraph;
class FrameGraphBlackboard;

/**
 * HDR raster path: clear → forward opaque (M5 minimal) → blit to swapchain.
 */
class RasterFeature final : public IRenderFeature {
public:
    RasterFeature() = default;
    ~RasterFeature() override;

    const char* FeatureDebugName() const noexcept override { return writer_names::kRasterFeature; }

    void OnRegister(const RenderCapabilities& caps) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    void SetMeshResolver(const IMeshHandleResolver* resolver) noexcept;

private:
    static rhi::ShaderGraphicsPipelineCreateInfo makeForwardOpaqueBaselineSpec();

    shaders::ShaderLibrary m_shaderLibrary;
    rhi::IDevice* m_pipelineDevice = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;

    bool m_diagLoggedForwardPipelineNull = false;
    bool m_diagLoggedMeshResolverNull = false;
    bool m_diagLoggedDrawListNull = false;
    uint32_t m_diagScheduleLogCounter = 0;
    bool m_diagLoggedPerViewOnce = false;
};

}  // namespace rendering
}  // namespace eyelib
