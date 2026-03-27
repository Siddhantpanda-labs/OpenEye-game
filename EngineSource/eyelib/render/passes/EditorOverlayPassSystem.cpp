#include "eyelib/render/passes/EditorOverlayPassSystem.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {

OutlineOverlayExecuteParams EditorOverlayPassSystem::BuildOutlineParams(const OutlineOverlayBuildParams& params) const {
    OutlineOverlayExecuteParams out{};
    out.build = params;
    return out;
}

EntityIdOverlayExecuteParams EditorOverlayPassSystem::BuildEntityIdParams(const EntityIdOverlayBuildParams& params) const {
    EntityIdOverlayExecuteParams out{};
    out.build = params;
    return out;
}

DebugLinesOverlayExecuteParams EditorOverlayPassSystem::BuildDebugLinesParams(const DebugLinesOverlayBuildParams& params) const {
    DebugLinesOverlayExecuteParams out{};
    out.build = params;
    out.camView.view = params.view;
    out.camView.projection = params.projection;
    out.camView.viewPos = math::Vec3(0, 0, 0);
    return out;
}

bool EditorOverlayPassSystem::ExecuteDebugLines(const DebugLinesOverlayExecuteParams& params) const {
    if (!params.build.device || !params.build.cmd || !params.build.renderTarget || params.build.width == 0 || params.build.height == 0 || !params.build.hasAnyLines) return false;
    if (params.build.drawLines) params.build.drawLines(params.build.cmd, params.build.renderTarget, params.camView, params.build.width, params.build.height);
    return true;
}

bool EditorOverlayPassSystem::ExecuteEntityId(const EntityIdOverlayExecuteParams& params) const {
    if (!params.build.device || !params.build.cmd || !params.build.entityIdRenderTarget || !params.build.entityIdShader) return false;

    params.build.cmd->SetRenderTarget(params.build.entityIdRenderTarget);
    params.build.cmd->SetViewport(0, 0, params.build.width, params.build.height);
    params.build.cmd->SetDepthTest(true);
    params.build.cmd->SetDepthWrite(false);
    params.build.cmd->SetCullFace(true);

    rhi::ClearValue clearId = {};
    clearId.colorUint = 0u;
    params.build.cmd->ClearRenderTarget(params.build.entityIdRenderTarget, clearId);

    params.build.entityIdShader->Bind(params.build.device);
    if (params.build.useTypedPush) {
        abi::WriteEntityIdViewProjectionPushConstants(params.build.cmd, params.build.camView.view, params.build.camView.projection);
    } else {
        params.build.entityIdShader->SetMat4("u_View", params.build.camView.view);
        params.build.entityIdShader->SetMat4("u_Projection", params.build.camView.projection);
    }

    if (params.build.drawItems) params.build.drawItems(params.build.cmd, params.build.entityIdShader, params.build.useTypedPush);

    params.build.entityIdShader->Unbind();
    params.build.cmd->SetDepthWrite(true);
    params.build.cmd->SetRenderTarget(nullptr);
    return true;
}

bool EditorOverlayPassSystem::ExecuteOutline(const OutlineOverlayExecuteParams& params) const {
    if (!params.build.device || !params.build.cmd || !params.build.hdrRenderTarget || !params.build.entityIdRenderTarget ||
        !params.build.outlineCompositeTemp || !params.build.outlineShader || params.build.selectedPackedId == 0) {
        return false;
    }

    params.build.device->Blit(params.build.hdrRenderTarget, params.build.outlineCompositeTemp, true);
    rhi::ITexture* sceneTex = params.build.outlineCompositeTemp->GetColorTexture(0);
    rhi::ITexture* idTex = params.build.entityIdRenderTarget->GetColorTexture(0);
    if (sceneTex) rhi::BarrierTextureForShaderRead(params.build.cmd, sceneTex);
    if (idTex) rhi::BarrierTextureForShaderRead(params.build.cmd, idTex);

    params.build.cmd->SetRenderTarget(params.build.hdrRenderTarget);
    params.build.cmd->SetViewport(0, 0, params.build.width, params.build.height);
    params.build.cmd->SetDepthTest(false);
    params.build.cmd->SetDepthWrite(false);
    params.build.cmd->SetCullFace(false);

    params.build.outlineShader->Bind(params.build.device);
    if (params.build.outlineSet && sceneTex && idTex) {
        if (params.build.bindDescriptorSet) params.build.bindDescriptorSet(params.build.cmd, params.build.outlineCompositeTemp, params.build.entityIdRenderTarget, params.build.outlineSet);
    } else {
        if (params.build.bindFallbackTextures) params.build.bindFallbackTextures(params.build.cmd, params.build.outlineCompositeTemp, params.build.entityIdRenderTarget);
    }

    const float invW = params.build.width > 0 ? 1.0f / static_cast<float>(params.build.width) : 0.0f;
    const float invH = params.build.height > 0 ? 1.0f / static_cast<float>(params.build.height) : 0.0f;
    if (params.build.writePush) {
        params.build.writePush(params.build.cmd, params.build.outlineShader, params.build.useTypedOutlinePush, params.build.selectedPackedId,
                               params.build.colorR, params.build.colorG, params.build.colorB, invW, invH);
    }

    rhi::IPipeline* pipe = params.build.outlineShader->GetPipelineRHI();
    if (pipe) params.build.cmd->DrawFullscreenQuad(pipe);

    params.build.outlineShader->Unbind();
    params.build.cmd->SetDepthTest(true);
    params.build.cmd->SetDepthWrite(true);
    params.build.cmd->SetCullFace(true);
    params.build.cmd->SetRenderTarget(nullptr);
    if (rhi::ITexture* outTex = params.build.hdrRenderTarget->GetColorTexture(0))
        rhi::BarrierTextureForShaderRead(params.build.cmd, outTex);
    return true;
}

}  // namespace render
}  // namespace eyelib

