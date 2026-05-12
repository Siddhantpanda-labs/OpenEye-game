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

class RasterFeature;  // see RasterFeature.h — supplies the default env cubemap fallback.

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
 * IBL probe ownership (post-IblProbeSwap):
 *  - This feature owns the `IblIrradianceCubemap` + `IblPrefilterCubemap` blackboard
 *    writer slots (writer name `kSkyboxFeature`). `RasterFeature` keeps `IblBrdfLut`.
 *  - When `Kind::Cubemap` is set, `OnUpload` invokes `IBL::GenerateIrradianceAndPrefilter`
 *    once per unique source-cubemap pointer to bake diffuse + GGX-prefiltered specular
 *    cubemaps; `OnSchedulePasses` publishes those baked textures.
 *  - When `Kind::Procedural` or `Kind::None`, the publishes use the bootstrap env cubemap
 *    returned by `RasterFeature::GetDefaultEnvCubemap()` (wired via `SetDefaultEnvProvider`
 *    by the pipeline at subscription time) so the lighting pass always has a valid sample.
 *  - Baked textures are owned by this feature and destroyed on shutdown or when a fresh
 *    source cubemap arrives.
 *
 * Ownership model:
 *  - Shader permutations + pipelines are owned by the feature's `ShaderLibrary` and
 *    destroyed in the destructor / `OnShutdown`.
 *  - The source cubemap texture is **not** owned — the public `api::rendering::Skybox`
 *    handle (driven by the editor / player) owns it. The feature only borrows the pointer
 *    for one frame at a time. Per-in-flight-frame descriptor sets live in `SkyboxPass`.
 *  - The persistent `SkyboxPass` is destroyed before the pipeline device is cleared
 *    (parallel to `RasterFeature::~RasterFeature`).
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
    void OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) override;
    void OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    /** Per-frame sky source. Called by the public façade from `RenderScene::skybox`. The
     *  feature copies the request; the cubemap pointer (if any) must remain alive until
     *  after the next `RenderWorld::RunFrame` completes. */
    void SetRequest(const SkyboxRequest& request) noexcept;

    const SkyboxRequest& GetRequest() const noexcept { return m_request; }

    /** Wire the fallback IBL provider used when `Kind::Procedural` / `Kind::None` is active.
     *  The pipeline calls this once after both features are constructed; `provider` may be
     *  null (the feature publishes nothing on the IBL slots in that case). Lifetime of
     *  `provider` must outlive this feature. */
    void SetDefaultEnvProvider(RasterFeature* provider) noexcept { m_defaultEnvProvider = provider; }

private:
    static rhi::ShaderGraphicsPipelineCreateInfo MakeCubemapSpec();
    static rhi::ShaderGraphicsPipelineCreateInfo MakeProceduralSpec();

    /** Destroy `m_bakedIrradiance` / `m_bakedPrefilter` on the supplied device (no-op when
     *  either is null). Called from the destructor / `OnShutdown` and when the source
     *  cubemap pointer changes between frames. */
    void DestroyBakedProbes(rhi::IDevice* device) noexcept;

    shaders::ShaderLibrary       m_shaderLibrary;
    rhi::IDevice*                m_pipelineDevice = nullptr;
    std::unique_ptr<SkyboxPass>  m_skyboxPass;
    SkyboxRequest                m_request{};

    RasterFeature*               m_defaultEnvProvider = nullptr;
    /** Source cubemap pointer used to produce `m_bakedIrradiance` / `m_bakedPrefilter`.
     *  Used as a cache key — when the request's cubemap changes between frames we destroy
     *  the previous bake and remake from the new source. */
    rhi::ITexture*               m_bakedSource     = nullptr;
    rhi::ITexture*               m_bakedIrradiance = nullptr;
    rhi::ITexture*               m_bakedPrefilter  = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
