#include "eyelib/rendering/features/raster/RasterFeature.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/core/DeferredDebugVis.h"
#include "eyelib/rendering/core/FrameContext.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/core/RenderContext.h"
#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/framegraph/passes/ForwardOpaquePass.h"
#include "eyelib/rendering/framegraph/passes/GBufferPass.h"
#include "eyelib/rendering/framegraph/passes/DeferredLightingPass.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/shaders/ShaderPermutationKey.h"

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

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
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
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
    gp.layoutKind = rhi::PipelineLayoutKind::RasterForwardMinimal;
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
    gp.sourcePolicy = rhi::ShaderSourcePolicy::CompileFromSource;
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

void RasterFeature::OnShutdown() {
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
}

void RasterFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    RenderResources* res = render.resources;
    if (!res || !render.recordingCommandList || !render.device)
        return;

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

    if (!m_diagLoggedPerViewOnce) {
        m_diagLoggedPerViewOnce = true;
        const auto& vp = perView.viewProjection;
        const auto& v = perView.view;
        const auto& p = perView.projection;
        core::Log::InfoF("PerView uploaded: viewPos=(%.3f,%.3f,%.3f) outputDims=%ux%u",
                         perView.viewPosition.x, perView.viewPosition.y, perView.viewPosition.z,
                         render.outputWidth, render.outputHeight);
        core::Log::InfoF("PerView view col0=(%.3f,%.3f,%.3f,%.3f) col3=(%.3f,%.3f,%.3f,%.3f)",
                         v[0][0], v[0][1], v[0][2], v[0][3], v[3][0], v[3][1], v[3][2], v[3][3]);
        core::Log::InfoF("PerView proj col0=(%.3f,%.3f,%.3f,%.3f) col2=(%.3f,%.3f,%.3f,%.3f) col3=(%.3f,%.3f,%.3f,%.3f)",
                         p[0][0], p[0][1], p[0][2], p[0][3], p[2][0], p[2][1], p[2][2], p[2][3],
                         p[3][0], p[3][1], p[3][2], p[3][3]);
        const glm::vec4 testWorld{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 clip = vp * testWorld;
        core::Log::InfoF("Test origin-clip: (%.3f,%.3f,%.3f,%.3f) ndc=(%.3f,%.3f,%.3f)",
                         clip.x, clip.y, clip.z, clip.w,
                         clip.w != 0 ? clip.x / clip.w : 0,
                         clip.w != 0 ? clip.y / clip.w : 0,
                         clip.w != 0 ? clip.z / clip.w : 0);
        if (RenderResources* r = render.resources) {
            if (rhi::IRenderTarget* rt = r->MainHdrRenderTarget()) {
                core::Log::InfoF("MainHdrRenderTarget dims=%ux%u hasDepth=%s",
                                 rt->GetWidth(), rt->GetHeight(),
                                 rt->GetDepthTexture() ? "yes" : "no");
            } else {
                core::Log::Warn("MainHdrRenderTarget is null during upload.");
            }
        }
    }

    DirectionalLightStd140 dir;
    dir.direction = {-0.35f, -0.85f, -0.25f, 0.0f}; // Sun-like slant
    dir.colorIntensity = {1.0f, 0.95f, 0.85f, 5.0f}; // Warm light, high intensity
    const bool uploadedDirLight = res->UploadDirectionalLightUniform(render.recordingCommandList, dir);

    bool uploadedPerObject = true;
    if (render.drawList)
        uploadedPerObject = res->UploadPerObjectBufferFromDrawList(render.recordingCommandList, *render.drawList);

    if (render.drawList && render.drawList->VisibleCount() > 0u) {
        static bool s_loggedFirstDrawItemClip = false;
        if (!s_loggedFirstDrawItemClip) {
            s_loggedFirstDrawItemClip = true;
            const DrawItem& it0 = render.drawList->items[0];
            const glm::vec4 worldCenter = it0.worldMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f);
            const glm::vec4 clipCenter = perView.projection * perView.view * worldCenter;
            core::Log::InfoF(
                "RasterFeature::OnUpload drawItem0 CPU clip test: materialId=%u meshHandle=%u transformIndex=%u "
                "worldCenter=(%.3f,%.3f,%.3f,%.3f) clip=(%.3f,%.3f,%.3f,%.3f) ndc=(%.3f,%.3f,%.3f)",
                it0.materialId,
                it0.meshHandle,
                it0.transformIndex,
                worldCenter.x, worldCenter.y, worldCenter.z, worldCenter.w,
                clipCenter.x, clipCenter.y, clipCenter.z, clipCenter.w,
                clipCenter.w != 0.f ? clipCenter.x / clipCenter.w : 0.f,
                clipCenter.w != 0.f ? clipCenter.y / clipCenter.w : 0.f,
                clipCenter.w != 0.f ? clipCenter.z / clipCenter.w : 0.f);
        }
    }

    if (!uploadedPerFrame || !uploadedDirLight || !uploadedPerObject) {
        static int s_uploadFailWarnCount = 0;
        if (s_uploadFailWarnCount < 8) {
            ++s_uploadFailWarnCount;
            core::Log::WarnF(
                "RasterFeature::OnUpload resource upload status: perFrame=%s perView=%s dirLight=%s perObject=%s visible=%u",
                uploadedPerFrame ? "ok" : "fail",
                uploadedPerView ? "ok" : "fail",
                uploadedDirLight ? "ok" : "fail",
                uploadedPerObject ? "ok" : "fail",
                render.drawList ? render.drawList->VisibleCount() : 0u);
        }
    }
}

void RasterFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (render.device) {
        m_shaderLibrary.WarmUpPipelines(render.device);
        m_pipelineDevice = render.device;
    }

    rhi::IPipeline* forwardPipeline = m_shaderLibrary.TryGetPipeline(kPermForwardOpaqueBaseline);

    if (!m_diagLoggedForwardPipelineNull && !forwardPipeline) {
        m_diagLoggedForwardPipelineNull = true;
        core::Log::Warn("RasterFeature: forward opaque pipeline is null after WarmUpPipelines; ForwardOpaquePass will be skipped.");
    }
    if (!m_diagLoggedMeshResolverNull && !m_meshResolver) {
        m_diagLoggedMeshResolverNull = true;
        core::Log::Warn("RasterFeature: mesh resolver is null; ForwardOpaquePass will be skipped.");
    }
    if (!m_diagLoggedDrawListNull && !render.drawList) {
        m_diagLoggedDrawListNull = true;
        core::Log::Warn("RasterFeature: render.drawList is null; ForwardOpaquePass will be skipped.");
    }

    const uint32_t visible = render.drawList ? render.drawList->VisibleCount() : 0u;
    if ((m_diagScheduleLogCounter++ % 600u) == 0u) {
        core::Log::InfoF("RasterFeature::OnSchedulePasses: visible=%u forwardPipeline=%s meshResolver=%s drawList=%s",
                         visible,
                         forwardPipeline ? "ok" : "null",
                         m_meshResolver ? "ok" : "null",
                         render.drawList ? "ok" : "null");
    }

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
#if OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS
        if (!(gbufferPipeline && lightingPipeline && m_meshResolver && render.drawList)) {
            static int s_deferredSkipLog = 0;
            if (s_deferredSkipLog < 10) {
                ++s_deferredSkipLog;
                core::Log::WarnF(
                    "[DeferredDebug] RasterFeature deferred passes NOT scheduled: gbufferPipeline=%s lightingPipeline=%s "
                    "meshResolver=%s drawList=%s",
                    gbufferPipeline ? "ok" : "null",
                    lightingPipeline ? "ok" : "null",
                    m_meshResolver ? "ok" : "null",
                    render.drawList ? "ok" : "null");
            }
        }
#endif
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

    if (!m_blitPass)
        m_blitPass = std::make_unique<BlitMainHdrToDefaultPass>(render.device, &blackboard);
    else
        m_blitPass->Reconfigure(render.device, &blackboard);
    graph.AddPassBorrowed(m_blitPass.get());
}

}  // namespace rendering
}  // namespace eyelib
