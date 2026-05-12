#include "eyelib/rendering/features/raster/SkyboxFeature.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/BlackboardWriterRegistry.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/lighting/ibl/IBL.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/shaders/ShaderPermutationKey.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace eyelib {
namespace rendering {

namespace {

const shaders::ShaderPermutationKey kPermSkyboxCubemap{
    shaders::ShaderPermutationKey::ProgramId::SkyboxCubemap, 1, 1};

const shaders::ShaderPermutationKey kPermSkyboxProcedural{
    shaders::ShaderPermutationKey::ProgramId::SkyboxProcedural, 1, 1};

}  // namespace

SkyboxFeature::~SkyboxFeature() {
    m_skyboxPass.reset();
    DestroyBakedProbes(m_pipelineDevice);
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_defaultEnvProvider = nullptr;
}

void SkyboxFeature::DestroyBakedProbes(rhi::IDevice* device) noexcept {
    if (device) {
        if (m_bakedIrradiance) device->DestroyTexture(m_bakedIrradiance);
        if (m_bakedPrefilter)  device->DestroyTexture(m_bakedPrefilter);
    }
    m_bakedIrradiance = nullptr;
    m_bakedPrefilter  = nullptr;
    m_bakedSource     = nullptr;
}

rhi::ShaderGraphicsPipelineCreateInfo SkyboxFeature::MakeCubemapSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "passes/sky/skybox_cubemap";
    // No vertex layout — full-screen triangle generated from gl_VertexIndex (matches the
    // deferred-lighting full-screen path).
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::IBL;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = false;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::LessEqual;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ShaderGraphicsPipelineCreateInfo SkyboxFeature::MakeProceduralSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "passes/sky/skybox_procedural";
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = false;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::LessEqual;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

void SkyboxFeature::OnRegister(const RenderCapabilities& caps) {
    (void)caps;
    m_shaderLibrary.ClearRegistrations();
    m_shaderLibrary.AddEntry(kPermSkyboxCubemap, MakeCubemapSpec());
    m_shaderLibrary.AddEntry(kPermSkyboxProcedural, MakeProceduralSpec());
}

void SkyboxFeature::OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) {
    (void)caps;
    // Ownership of the diffuse + specular probe slots lives here so the per-scene cubemap
    // bake can drive the lighting pass's IBL term. BRDF LUT stays on `RasterFeature`.
    (void)registry.TryReserve(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature);
    (void)registry.TryReserve(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature);
}

void SkyboxFeature::OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) {
    (void)registry.TryRelease(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature);
    (void)registry.TryRelease(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature);
}

void SkyboxFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    (void)frame;
    // Per-scene IBL probe bake. The static helper records into `render.recordingCommandList`
    // (matches the legacy bake site), so this must run during the upload phase before the
    // frame graph executes the lighting pass that consumes the published probes.
    if (!render.device || !render.recordingCommandList) return;

    rhi::ITexture* src = (m_request.kind == SkyboxRequest::Kind::Cubemap) ? m_request.cubemap : nullptr;
    if (!src || !src->IsGpuBindable()) {
        // No per-scene source — keep any previous bake destroyed so the fallback env is
        // exposed at schedule time. Destroying eagerly here (rather than at the next
        // successful bake) avoids holding extra GPU memory across procedural-sky frames.
        if (m_bakedSource) DestroyBakedProbes(render.device);
        return;
    }

    // Skip the rebake when the source pointer is unchanged. A new cubemap (different asset
    // or hot-swap) shows up as a different `ITexture*` from `EditorSkyboxCache::GetOrLoad`.
    if (src == m_bakedSource && m_bakedIrradiance && m_bakedPrefilter) return;

    if (m_bakedSource) DestroyBakedProbes(render.device);

    rhi::ITexture* irradiance = nullptr;
    rhi::ITexture* prefilter  = nullptr;
    const bool ok = IBL::GenerateIrradianceAndPrefilter(render.device, src,
                                                       /*irradianceRes=*/32u,
                                                       /*prefilterRes=*/512u,
                                                       irradiance, prefilter);
    if (!ok || !irradiance || !prefilter) {
        if (irradiance) render.device->DestroyTexture(irradiance);
        if (prefilter)  render.device->DestroyTexture(prefilter);
        static int s_warnCount = 0;
        if (s_warnCount < 4) {
            ++s_warnCount;
            core::Log::Warn("SkyboxFeature: IBL probe bake failed; falling back to default env for this scene.");
        }
        return;
    }

    m_bakedSource     = src;
    m_bakedIrradiance = irradiance;
    m_bakedPrefilter  = prefilter;
}

void SkyboxFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (!render.device) return;

    m_shaderLibrary.WarmUpPipelines(render.device);
    m_pipelineDevice = render.device;

    // Publish IBL diffuse + specular probes every frame regardless of `Kind`. Lighting
    // expects both slots populated (the previous design had `RasterFeature` covering both
    // with the default env). Preference order: baked per-scene cubemap → fallback env from
    // `RasterFeature::GetDefaultEnvCubemap()`. When neither is available we skip publishing
    // so the deferred lighting pass gets a clean "no IBL" signal rather than a stale texture.
    rhi::ITexture* iblSource = m_bakedIrradiance;
    rhi::ITexture* iblSpec   = m_bakedPrefilter;
    if (!iblSource || !iblSpec) {
        rhi::ITexture* fallback = m_defaultEnvProvider ? m_defaultEnvProvider->GetDefaultEnvCubemap() : nullptr;
        iblSource = fallback;
        iblSpec   = fallback;
    }
    if (iblSource)
        blackboard.PublishTexture(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature, iblSource);
    if (iblSpec)
        blackboard.PublishTexture(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature, iblSpec);

    if (m_request.kind == SkyboxRequest::Kind::None) return;

    rhi::IPipeline* cubemapPipeline = m_shaderLibrary.TryGetPipeline(kPermSkyboxCubemap);
    rhi::IPipeline* proceduralPipeline = m_shaderLibrary.TryGetPipeline(kPermSkyboxProcedural);

    // If the active kind has no pipeline yet (shader compile still warming up), skip — the
    // pass will be appended on a subsequent frame once the pipeline is ready.
    const bool needCubemap = (m_request.kind == SkyboxRequest::Kind::Cubemap);
    if (needCubemap && !cubemapPipeline) return;
    if (!needCubemap && !proceduralPipeline) return;

    if (!m_skyboxPass)
        m_skyboxPass = std::make_unique<SkyboxPass>(&blackboard, render.device, cubemapPipeline, proceduralPipeline);
    else
        m_skyboxPass->Reconfigure(&blackboard, render.device, cubemapPipeline, proceduralPipeline);

    // Inverse view / projection from the active camera. Both shader variants reconstruct a
    // world-space view direction from these matrices and `vUv`. Identity is harmless when no
    // camera is bound (sky still renders, but with a degenerate transform; the public façade
    // never reaches this point without a camera in practice).
    math::Mat4 invView{1.0f};
    math::Mat4 invProj{1.0f};
    if (const CameraView* cam = render.sceneView ? render.sceneView->TryGetCamera() : nullptr) {
        invView = glm::inverse(cam->view);
        invProj = glm::inverse(cam->projection);
    }
    m_skyboxPass->SetFrameState(m_request, invView, invProj);

    graph.AddPassBorrowed(m_skyboxPass.get());
}

void SkyboxFeature::OnShutdown() {
    m_skyboxPass.reset();
    DestroyBakedProbes(m_pipelineDevice);
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_request = {};
    m_defaultEnvProvider = nullptr;
}

void SkyboxFeature::SetRequest(const SkyboxRequest& request) noexcept {
    m_request = request;
    if (m_request.brightness < 0.0f) m_request.brightness = 0.0f;
}

}  // namespace rendering
}  // namespace eyelib
