#include "eyelib/rendering/features/raster/SkyboxFeature.h"

#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
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
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
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

void SkyboxFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    (void)frame; (void)render;
    // Push constants are filled at Execute time from the cached request + camera matrices.
}

void SkyboxFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (m_request.kind == SkyboxRequest::Kind::None) return;
    if (!render.device) return;

    m_shaderLibrary.WarmUpPipelines(render.device);
    m_pipelineDevice = render.device;

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
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_request = {};
}

void SkyboxFeature::SetRequest(const SkyboxRequest& request) noexcept {
    m_request = request;
    if (m_request.brightness < 0.0f) m_request.brightness = 0.0f;
}

}  // namespace rendering
}  // namespace eyelib
