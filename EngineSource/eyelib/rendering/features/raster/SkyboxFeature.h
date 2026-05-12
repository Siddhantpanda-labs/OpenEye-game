#pragma once

#include "eyelib/rendering/features/raster/SkyboxPass.h"
#include "eyelib/rendering/features/raster/SkyboxRequest.h"
#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/rendering/shaders/ShaderLibrary.h"

#include <memory>

namespace eyelib {
namespace rendering {

namespace rhi {
struct IDevice;
struct IPipeline;
}

/**
 * Optional sky feature for the new render module. Renders one of three sources per frame:
 *  - `Kind::None`       — feature is fully inert (no pass scheduled).
 *  - `Kind::Procedural` — analytic gradient sky (no texture input).
 *  - `Kind::Cubemap`    — samples a per-scene environment cubemap supplied by the caller.
 *
 * Scheduling: `SchedulingOrder() = 50` so the pass runs after `RasterFeature` (=0) and
 * before `EditorFeature` (=100). Inside a frame this materialises as:
 *   GBuffer → DeferredLighting → **Skybox** → editor debug overlays → blit/publish.
 *
 * Ownership model:
 *  - Shader permutations + pipelines are owned by the feature's `ShaderLibrary` and
 *    destroyed in the destructor / `OnShutdown`.
 *  - The cubemap texture is **not** owned — the public `api::rendering::Skybox` handle
 *    (driven by the editor / player) owns it. The feature only borrows a pointer for
 *    one frame at a time. Per-in-flight-frame descriptor sets live in `SkyboxPass`.
 *  - The persistent `SkyboxPass` is destroyed before the pipeline device is cleared
 *    (parallel to `RasterFeature::~RasterFeature`).
 *
 * Per-scene IBL probe swap (re-baking irradiance + prefilter from the user-supplied
 * cubemap and republishing on `IblIrradianceCubemap` / `IblPrefilterCubemap`) is the
 * documented follow-up — tracked in `documentation/Render_Module/Render_steps_todo.md`.
 * Today the IBL slots remain published by `IblDefaultProbes` (neutral env).
 */
class SkyboxFeature final : public IRenderFeature {
public:
    SkyboxFeature() = default;
    ~SkyboxFeature() override;

    SkyboxFeature(const SkyboxFeature&) = delete;
    SkyboxFeature& operator=(const SkyboxFeature&) = delete;

    const char* FeatureDebugName() const noexcept override { return writer_names::kSkyboxFeature; }
    int SchedulingOrder() const noexcept override { return 50; }

    void OnRegister(const RenderCapabilities& caps) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    /** Per-frame sky source. Called by the public façade from `RenderScene::skybox`. The
     *  feature copies the request; the cubemap pointer (if any) must remain alive until
     *  after the next `RenderWorld::RunFrame` completes. */
    void SetRequest(const SkyboxRequest& request) noexcept;

    const SkyboxRequest& GetRequest() const noexcept { return m_request; }

private:
    static rhi::ShaderGraphicsPipelineCreateInfo MakeCubemapSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo MakeProceduralSpec();

    shaders::ShaderLibrary       m_shaderLibrary;
    rhi::IDevice*                m_pipelineDevice = nullptr;
    std::unique_ptr<SkyboxPass>  m_skyboxPass;
    SkyboxRequest                m_request{};
};

}  // namespace rendering
}  // namespace eyelib
