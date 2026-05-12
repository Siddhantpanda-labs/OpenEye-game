#pragma once

#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/framegraph/passes/PublishOffscreenOutputPass.h"
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
    int SchedulingOrder() const noexcept override { return 0; }

    void OnRegister(const RenderCapabilities& caps) override;
    void OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) override;
    void OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    void SetMeshResolver(const IMeshHandleResolver* resolver) noexcept;
    void SetRenderMode(RenderMode mode) noexcept { m_mode = mode; }

    /** Bootstrap default environment cubemap (neutral / blue-ish). Owned by the internal
     *  `IblDefaultProbes` and valid from the first `OnSchedulePasses` until `OnShutdown`.
     *  Returns nullptr before the device is bound. Consumed by `SkyboxFeature` as the
     *  fallback IBL source when no per-scene cubemap is set (procedural sky or `Kind::None`).
     *  BRDF LUT publication stays inside this feature; see `OnSchedulePasses`. */
    rhi::ITexture* GetDefaultEnvCubemap() noexcept { return m_iblDefaultProbes.EnvCubemap(); }

    /**
     * Gate the final `BlitMainHdrToDefaultPass` (swapchain blit + post-blit overlay invocation).
     * When `false`, the pass is **not** appended to the frame graph — used by editor-style
     * pipelines created via `InitializeWithSharedDevice` where 3 pipelines render to offscreen
     * RTs per frame and a single, caller-driven swapchain compose runs after all of them.
     * When `true` (default), the blit pass runs every frame — the standalone-player one-pipeline
     * shape that owns the swapchain. Must be set before `OnSchedulePasses` runs (i.e. before
     * the first `RenderWorld::RunFrame`); changes mid-frame are ignored for that frame.
     */
    void SetPresentationBlitEnabled(bool enabled) noexcept { m_presentationBlitEnabled = enabled; }
    bool IsPresentationBlitEnabled() const noexcept { return m_presentationBlitEnabled; }

private:
    static rhi::ShaderGraphicsPipelineCreateInfo makeForwardOpaqueBaselineSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo makeGBufferOpaqueBaselineSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo makeDeferredLightingSpec();

    RenderMode m_mode = RenderMode::Deferred;
    // True when this feature instance owns the swapchain blit (standalone player / window-owning
    // pipeline). False for editor shared-device offscreen pipelines — see SetPresentationBlitEnabled.
    bool m_presentationBlitEnabled = true;
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
    // Finalising barrier-only pass scheduled when m_presentationBlitEnabled == false. Materialises
    // the offscreen-output layout contract (main HDR color0 → ShaderResource at end-of-frame) so
    // host compositors (e.g. editor ImGui viewports) can sample the output without extra sync.
    std::unique_ptr<PublishOffscreenOutputPass> m_publishOffscreenPass;
};

}  // namespace rendering
}  // namespace eyelib
