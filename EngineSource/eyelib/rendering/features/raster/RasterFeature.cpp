#include "eyelib/rendering/features/raster/RasterFeature.h"

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
    (void)res->UploadPerFrameUniform(render.recordingCommandList, perFrame);

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
    (void)res->UploadPerViewUniform(render.recordingCommandList, perView);

    DirectionalLightStd140 dir{};
    {
        glm::vec3 d(dir.direction.x, dir.direction.y, dir.direction.z);
        d = glm::normalize(d);
        dir.direction = {d.x, d.y, d.z, 0.f};
    }
    (void)res->UploadDirectionalLightUniform(render.recordingCommandList, dir);

    if (render.drawList)
        (void)res->UploadPerObjectBufferFromDrawList(render.recordingCommandList, *render.drawList);
}

void RasterFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (render.device) {
        m_shaderLibrary.WarmUpPipelines(render.device);
        m_pipelineDevice = render.device;
    }

    rhi::IPipeline* forwardPipeline = m_shaderLibrary.TryGetPipeline(kPermForwardOpaqueM5Baseline);

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
