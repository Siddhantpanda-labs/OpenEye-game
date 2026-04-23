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
#include "eyelib/rendering/framegraph/passes/ClearBlackboardRenderTargetPass.h"
#include "eyelib/rendering/framegraph/passes/ForwardOpaquePass.h"
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

const shaders::ShaderPermutationKey kPermForwardOpaqueM5Baseline{
    shaders::ShaderPermutationKey::ProgramId::RasterForwardOpaque, 1, 1};

}  // namespace

RasterFeature::~RasterFeature() {
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
    gp.layoutKind = rhi::PipelineLayoutKind::RasterForwardM5Minimal;
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

void RasterFeature::OnRegister(const RenderCapabilities& caps) {
    (void)caps;
    m_shaderLibrary.ClearRegistrations();
    m_shaderLibrary.AddEntry(kPermForwardOpaqueM5Baseline, makeForwardOpaqueBaselineSpec());
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

    DirectionalLightStd140 dir{};
    {
        glm::vec3 d(dir.direction.x, dir.direction.y, dir.direction.z);
        d = glm::normalize(d);
        dir.direction = {d.x, d.y, d.z, 0.f};
    }
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

    rhi::IPipeline* forwardPipeline = m_shaderLibrary.TryGetPipeline(kPermForwardOpaqueM5Baseline);

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
    if ((m_diagScheduleLogCounter++ % 120u) == 0u) {
        core::Log::InfoF("RasterFeature::OnSchedulePasses: visible=%u forwardPipeline=%s meshResolver=%s drawList=%s",
                         visible,
                         forwardPipeline ? "ok" : "null",
                         m_meshResolver ? "ok" : "null",
                         render.drawList ? "ok" : "null");
    }

    rhi::ClearValue clear{};
    clear.color[0] = 0.12f;
    clear.color[1] = 0.14f;
    clear.color[2] = 0.20f;
    clear.color[3] = 1.0f;
    clear.depth = 1.0f;
    graph.AddPass(std::make_unique<ClearBlackboardRenderTargetPass>(&blackboard, BlackboardSlot::MainHdrRenderTarget, clear));

    if (forwardPipeline && m_meshResolver && render.drawList)
        graph.AddPass(std::make_unique<ForwardOpaquePass>(&blackboard, BlackboardSlot::MainHdrRenderTarget, render.device, forwardPipeline,
                                                          m_meshResolver, render.drawList));

    graph.AddPass(std::make_unique<BlitMainHdrToDefaultPass>(render.device, &blackboard));
}

}  // namespace rendering
}  // namespace eyelib
