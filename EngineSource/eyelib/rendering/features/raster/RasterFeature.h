#pragma once

#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/features/raster/DeferredLightingPass.h"
#include "eyelib/rendering/features/raster/ForwardOpaquePass.h"
#include "eyelib/rendering/features/raster/GBufferPass.h"
#include "eyelib/rendering/lighting/ibl/IblDefaultProbes.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/rendering/shaders/ShaderLibrary.h"
#include <memory>

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;

enum class RenderMode { Forward, Deferred };

namespace rhi {
struct IDevice;
struct IPipeline;
}

class FrameGraph;
class FrameGraphBlackboard;

/**
 * HDR raster path: clear → forward opaque (forward minimal) → blit to swapchain.
 */
class RasterFeature final : public IRenderFeature {
public:
    RasterFeature() = default;
    ~RasterFeature() override;

    const char* FeatureDebugName() const noexcept override { return writer_names::kRasterFeature; }

    void OnRegister(const RenderCapabilities& caps) override;
    void OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) override;
    void OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    void SetMeshResolver(const IMeshHandleResolver* resolver) noexcept;
    void SetRenderMode(RenderMode mode) noexcept { m_mode = mode; }

private:
    static rhi::ShaderGraphicsPipelineCreateInfo makeForwardOpaqueBaselineSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo makeGBufferOpaqueBaselineSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo makeDeferredLightingSpec();

    RenderMode m_mode = RenderMode::Deferred;
    shaders::ShaderLibrary m_shaderLibrary;
    rhi::IDevice* m_pipelineDevice = nullptr;
    rhi::IDevice* m_iblDevice = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    IblDefaultProbes m_iblDefaultProbes;

    // Persistent pass objects — owned here so their descriptor sets survive across frames.
    std::unique_ptr<GBufferPass>            m_gbufferPass;
    std::unique_ptr<DeferredLightingPass>   m_deferredLightingPass;
    std::unique_ptr<ForwardOpaquePass>      m_forwardOpaquePass;
    std::unique_ptr<BlitMainHdrToDefaultPass> m_blitPass;
};

}  // namespace rendering
}  // namespace eyelib
