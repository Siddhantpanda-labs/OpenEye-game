#include "eyelib/render/passes/SkyboxPassSystem.h"
#include "eyelib/core/Log.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/resources/Shader.h"
#include <glm/gtc/matrix_inverse.hpp>

namespace eyelib {
namespace render {

SkyboxPassExecuteParams SkyboxPassSystem::BuildParams(const SkyboxPassBuildParams& params) const {
    SkyboxPassExecuteParams out{};
    out.build = params;
    out.inverseView = glm::inverse(params.camera.view);
    out.inverseProjection = glm::inverse(params.camera.projection);
    return out;
}

bool SkyboxPassSystem::ExecuteCubemap(const SkyboxPassExecuteParams& params) {
    Shader* const shader = params.build.shader;
    if (!shader || !shader->IsValid()) return false;
    rhi::ICommandList* cmd = params.build.cmd;
    rhi::IRenderTarget* rt = params.build.renderTarget;
    if (cmd && rt) {
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    }
    if (rt == params.build.hdrRenderTarget && params.build.useGbufferDepth && params.build.gbufferRenderTarget &&
        params.build.device && params.build.hdrRenderTarget)
        params.build.device->BlitDepthOnly(params.build.gbufferRenderTarget, params.build.hdrRenderTarget);
    if (cmd && rt)
        cmd->SetRenderTarget(rt);
    if (cmd) {
        cmd->SetCullFace(false);
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(false);
    }
    shader->Bind(params.build.device);
    if (params.build.isVulkan && cmd) {
        abi::WriteSkyboxInversePushConstants(cmd, params.inverseView, params.inverseProjection);
    } else {
        shader->SetMat4("u_InverseViewMatrix", params.inverseView);
        shader->SetMat4("u_InverseProjectionMatrix", params.inverseProjection);
    }
    shader->SetInt("u_Skybox", 0);
    if (!params.build.frameSet || !params.build.postSet) {
        core::Log::InfoF("Skybox cubemap: skip draw (frameSet=%p postSet=%p)", (void*)params.build.frameSet, (void*)params.build.postSet);
        return false;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (!cmd || !pipe) {
        core::Log::InfoF("Skybox cubemap: skip draw (cmd=%p pipe=%p)", (void*)cmd, (void*)pipe);
        return false;
    }
    cmd->BindPipeline(pipe);
    rhi::IDescriptorSet* sets[2] = { params.build.frameSet, params.build.postSet };
    cmd->BindDescriptorSets(0, 2, sets);
    if (params.build.viewportWidth == 0 || params.build.viewportHeight == 0) {
        core::Log::WarnF("Skybox cubemap: zero viewport vpW=%u vpH=%u (draw skipped)", params.build.viewportWidth, params.build.viewportHeight);
        return false;
    }
    cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    cmd->DrawFullscreenQuad(pipe);
    shader->Unbind();
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetRenderTarget(nullptr);
    }
    return true;
}

bool SkyboxPassSystem::ExecuteProcedural(const SkyboxPassExecuteParams& params) {
    Shader* const shader = params.build.shader;
    if (!shader || !shader->IsValid()) return false;
    rhi::ICommandList* cmd = params.build.cmd;
    rhi::IRenderTarget* rt = params.build.renderTarget;
    if (cmd && rt) {
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    }
    if (rt == params.build.hdrRenderTarget && params.build.useGbufferDepth && params.build.gbufferRenderTarget &&
        params.build.device && params.build.hdrRenderTarget)
        params.build.device->BlitDepthOnly(params.build.gbufferRenderTarget, params.build.hdrRenderTarget);
    if (cmd && rt)
        cmd->SetRenderTarget(rt);
    if (cmd)
        cmd->SetCullFace(false);
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(false);
    }

    shader->Bind(params.build.device);
    if (params.build.isVulkan && cmd) {
        abi::WriteSkyboxInversePushConstants(cmd, params.inverseView, params.inverseProjection);
    } else {
        shader->SetMat4("u_InverseViewMatrix", params.inverseView);
        shader->SetMat4("u_InverseProjectionMatrix", params.inverseProjection);
    }
    if (!params.build.frameSet || !params.build.postSet) {
        core::Log::InfoF("Skybox procedural: skip draw (frameSet=%p postSet=%p)", (void*)params.build.frameSet, (void*)params.build.postSet);
        return false;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (!cmd || !pipe) {
        core::Log::InfoF("Skybox procedural: skip draw (cmd=%p pipe=%p)", (void*)cmd, (void*)pipe);
        return false;
    }
    cmd->BindPipeline(pipe);
    rhi::IDescriptorSet* sets[2] = { params.build.frameSet, params.build.postSet };
    cmd->BindDescriptorSets(0, 2, sets);
    if (params.build.viewportWidth == 0 || params.build.viewportHeight == 0) {
        core::Log::WarnF("Skybox procedural: zero viewport vpW=%u vpH=%u (draw skipped)", params.build.viewportWidth, params.build.viewportHeight);
        return false;
    }
    cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    cmd->DrawFullscreenQuad(pipe);

    shader->Unbind();
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetRenderTarget(nullptr);
    }
    return true;
}

}  // namespace render
}  // namespace eyelib

