#include "eyelib/rendering/features/raster/RasterFeature.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/core/FrameContext.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/core/RenderContext.h"
#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/features/raster/ForwardOpaquePass.h"
#include "eyelib/rendering/features/raster/GBufferPass.h"
#include "eyelib/rendering/features/raster/DeferredLightingPass.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/lighting/SceneLighting.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/shaders/ShaderPermutationKey.h"

namespace eyelib {
namespace rendering {

namespace {

const shaders::ShaderPermutationKey kPermForwardOpaqueBaseline{
    shaders::ShaderPermutationKey::ProgramId::RasterForwardOpaque, 1, 1};

const shaders::ShaderPermutationKey kPermGBufferOpaqueBaseline{
    shaders::ShaderPermutationKey::ProgramId::RasterGBufferOpaque, 1, 1};

const shaders::ShaderPermutationKey kPermDeferredLightingBaseline{
    shaders::ShaderPermutationKey::ProgramId::RasterDeferredLighting, 1, 1};

}  // namespace

RasterFeature::~RasterFeature() {
    // Persistent passes must be destroyed before the pipeline device is cleared.
    m_gbufferPass.reset();
    m_deferredLightingPass.reset();
    m_forwardOpaquePass.reset();
    m_blitPass.reset();
    m_publishOffscreenPass.reset();
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_iblDefaultProbes.Shutdown(m_iblDevice);
    m_iblDevice = nullptr;
    m_pipelineDevice = nullptr;
}

void RasterFeature::SetMeshResolver(const IMeshHandleResolver* resolver) noexcept {
    m_meshResolver = resolver;
}

rhi::ShaderGraphicsPipelineCreateInfo RasterFeature::makeForwardOpaqueBaselineSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "raster/forward/forward_opaque";
    gp.vertexLayout = Mesh::GetVertexLayoutRHI();
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::RasterForwardMinimal;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = true;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::Less;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ShaderGraphicsPipelineCreateInfo RasterFeature::makeGBufferOpaqueBaselineSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "raster/deferred/gbuffer_static";
    gp.vertexLayout = Mesh::GetVertexLayoutRHI();
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::RasterGBuffer;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = true;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::Less;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA8);
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.color.push_back(rhi::RHITextureFormat::RGBA8);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ShaderGraphicsPipelineCreateInfo RasterFeature::makeDeferredLightingSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    // Both .vert and .frag live under raster/deferred/deferred_lighting_analytic in the rendering shader root.
    // The engine resolves: <assetsRoot>/shaders/rendering/vulkan/<assetPath>.vert / .frag (or .spv).
    gp.assetPath = "raster/deferred/deferred_lighting_analytic";
    // No vertex layout — full-screen triangle generated from gl_VertexIndex, no VBO.
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::RasterDeferredLighting;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = false;
    gp.depthStencilOverride.depthWrite = false;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::None;
    gp.attachmentFormats = att;
    return gp;
}


void RasterFeature::OnRegister(const RenderCapabilities& caps) {
    (void)caps;
    m_shaderLibrary.ClearRegistrations();
    m_shaderLibrary.AddEntry(kPermForwardOpaqueBaseline, makeForwardOpaqueBaselineSpec());
    m_shaderLibrary.AddEntry(kPermGBufferOpaqueBaseline, makeGBufferOpaqueBaselineSpec());
    m_shaderLibrary.AddEntry(kPermDeferredLightingBaseline, makeDeferredLightingSpec());
}

void RasterFeature::OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) {
    (void)caps;
    // IblIrradianceCubemap + IblPrefilterCubemap are now owned by `SkyboxFeature` so it can
    // publish per-scene baked probes when a `Kind::Cubemap` sky is set (and fall back to this
    // feature's default env otherwise — see `RasterFeature::GetDefaultEnvCubemap`). Only the
    // BRDF LUT is invariant per scene and stays with raster.
    (void)registry.TryReserve(BlackboardSlot::IblBrdfLut, writer_names::kLightingIbl);
}

void RasterFeature::OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) {
    (void)registry.TryRelease(BlackboardSlot::IblBrdfLut, writer_names::kLightingIbl);
}

void RasterFeature::OnShutdown() {
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_iblDefaultProbes.Shutdown(m_iblDevice);
    m_iblDevice = nullptr;
    m_pipelineDevice = nullptr;
}

void RasterFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    RenderResources* res = render.resources;
    if (!res || !render.recordingCommandList || !render.device)
        return;
    m_iblDevice = render.device;
    m_iblDefaultProbes.Ensure(render.device);

    res->EnsureUniformBuffers(render.device);

    PerFrameUniformStd140 perFrame{};
    perFrame.frameIndex = static_cast<uint32_t>(frame.frameIndex & 0xFFFFFFFFull);
    perFrame.deltaTime = frame.deltaSeconds;
    const bool uploadedPerFrame = res->UploadPerFrameUniform(render.recordingCommandList, perFrame);

    PerViewUniformStd140 perView{};
    if (const CameraView* cam = render.sceneView ? render.sceneView->TryGetCamera() : nullptr) {
        perView.view = cam->view;
        perView.projection = cam->projection;
        perView.viewProjection = cam->projection * cam->view;
        perView.viewPosition = {cam->viewPos.x, cam->viewPos.y, cam->viewPos.z, 1.0f};
    }
    const float w = static_cast<float>((render.outputWidth > 0u) ? render.outputWidth : 1u);
    const float h = static_cast<float>((render.outputHeight > 0u) ? render.outputHeight : 1u);
    perView.viewport = {w, h, 1.0f / w, 1.0f / h};
    const bool uploadedPerView = res->UploadPerViewUniform(render.recordingCommandList, perView);
    if (!uploadedPerView) {
        static int s_perViewUploadFailWarnCount = 0;
        if (s_perViewUploadFailWarnCount < 8) {
            ++s_perViewUploadFailWarnCount;
            core::Log::WarnF(
                "RasterFeature::OnUpload PerView upload failed: cmd=%s outputDims=%ux%u sceneView=%s camera=%s",
                render.recordingCommandList ? "ok" : "null",
                render.outputWidth,
                render.outputHeight,
                render.sceneView ? "ok" : "null",
                (render.sceneView && render.sceneView->TryGetCamera()) ? "ok" : "null");
        }
    }

    DirectionalLightStd140 dir;
    const DirectionalLightSceneState sceneLight = SceneLighting::GetDirectionalLight();
    dir.direction = {sceneLight.direction.x, sceneLight.direction.y, sceneLight.direction.z, 0.0f};
    dir.colorIntensity = {sceneLight.color.x, sceneLight.color.y, sceneLight.color.z, sceneLight.intensity};
    const bool uploadedDirLight = res->UploadDirectionalLightUniform(render.recordingCommandList, dir);

    bool uploadedPerObject = true;
    if (render.drawList)
        uploadedPerObject = res->UploadPerObjectBufferFromDrawList(render.recordingCommandList, *render.drawList);
    const bool uploadedMaterialTable = render.drawList
        ? res->UploadMaterialTableFromDrawList(render.recordingCommandList, *render.drawList)
        : true;

    if (!uploadedPerFrame || !uploadedDirLight || !uploadedPerObject || !uploadedMaterialTable) {
        static int s_uploadFailWarnCount = 0;
        if (s_uploadFailWarnCount < 8) {
            ++s_uploadFailWarnCount;
            core::Log::WarnF(
                "RasterFeature::OnUpload resource upload status: perFrame=%s perView=%s dirLight=%s perObject=%s "
                "materialTable=%s visible=%u",
                uploadedPerFrame ? "ok" : "fail",
                uploadedPerView ? "ok" : "fail",
                uploadedDirLight ? "ok" : "fail",
                uploadedPerObject ? "ok" : "fail",
                uploadedMaterialTable ? "ok" : "fail",
                render.drawList ? render.drawList->VisibleCount() : 0u);
        }
    }
}

void RasterFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (render.device) {
        m_shaderLibrary.WarmUpPipelines(render.device);
        m_pipelineDevice = render.device;
        if (!m_iblDevice) m_iblDevice = render.device;
    }
    // Irradiance + prefilter publishes moved to `SkyboxFeature::OnSchedulePasses`; that feature
    // either publishes per-scene baked probes (`Kind::Cubemap`) or falls back to the env returned
    // by `GetDefaultEnvCubemap()` above (procedural / none). Keep BRDF LUT here — it never changes
    // across scenes and the LUT loader lives in `IblDefaultProbes` next to its bootstrap.
    if (rhi::ITexture* iblLut = m_iblDefaultProbes.BrdfLut())
        blackboard.PublishTexture(BlackboardSlot::IblBrdfLut, writer_names::kLightingIbl, iblLut);

    rhi::IPipeline* forwardPipeline = m_shaderLibrary.TryGetPipeline(kPermForwardOpaqueBaseline);

    if (m_mode == RenderMode::Forward) {
        if (forwardPipeline && m_meshResolver && render.drawList) {
            if (!m_forwardOpaquePass)
                m_forwardOpaquePass = std::make_unique<ForwardOpaquePass>(
                    &blackboard, BlackboardSlot::MainHdrRenderTarget, render.device, forwardPipeline,
                    m_meshResolver, render.drawList);
            else
                m_forwardOpaquePass->Reconfigure(&blackboard, BlackboardSlot::MainHdrRenderTarget,
                                                 forwardPipeline, m_meshResolver, render.drawList);
            graph.AddPassBorrowed(m_forwardOpaquePass.get());
        }
    } else {
        rhi::IPipeline* gbufferPipeline = m_shaderLibrary.TryGetPipeline(kPermGBufferOpaqueBaseline);
        rhi::IPipeline* lightingPipeline = m_shaderLibrary.TryGetPipeline(kPermDeferredLightingBaseline);
        if (gbufferPipeline && lightingPipeline && m_meshResolver && render.drawList) {
            if (!m_gbufferPass)
                m_gbufferPass = std::make_unique<GBufferPass>(
                    &blackboard, render.device, gbufferPipeline, m_meshResolver, render.drawList);
            else
                m_gbufferPass->Reconfigure(&blackboard, gbufferPipeline, m_meshResolver, render.drawList);

            if (!m_deferredLightingPass)
                m_deferredLightingPass = std::make_unique<DeferredLightingPass>(
                    &blackboard, render.device, lightingPipeline);
            else
                m_deferredLightingPass->Reconfigure(&blackboard, lightingPipeline);

            graph.AddPassBorrowed(m_gbufferPass.get());
            graph.AddPassBorrowed(m_deferredLightingPass.get());
        }
    }

    // Presentation blit + post-blit overlay invocation is the swapchain owner's responsibility.
    // Editor-style shared-device pipelines opt out (see SetPresentationBlitEnabled) so the
    // editor can run multiple offscreen pipelines per device frame and drive its own
    // swapchain compose afterwards. In that offscreen shape the pipeline must still leave its
    // main HDR color in ShaderResource state at end-of-frame so the host compositor (ImGui
    // viewport sampling) can bind it as a sampled image without extra synchronisation — that
    // contract is materialised by PublishOffscreenOutputPass appended below.
    if (m_presentationBlitEnabled) {
        if (!m_blitPass)
            m_blitPass = std::make_unique<BlitMainHdrToDefaultPass>(render.device, &blackboard);
        else
            m_blitPass->Reconfigure(render.device, &blackboard);
        graph.AddPassBorrowed(m_blitPass.get());
    } else {
        if (!m_publishOffscreenPass)
            m_publishOffscreenPass = std::make_unique<PublishOffscreenOutputPass>(BlackboardSlot::MainHdrRenderTarget);
        else
            m_publishOffscreenPass->Reconfigure(BlackboardSlot::MainHdrRenderTarget);
        graph.AddPassBorrowed(m_publishOffscreenPass.get());
    }
}

}  // namespace rendering
}  // namespace eyelib
