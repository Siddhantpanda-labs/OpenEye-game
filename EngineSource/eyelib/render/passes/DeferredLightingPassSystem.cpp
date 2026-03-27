#include "eyelib/render/passes/DeferredLightingPassSystem.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/ibl/IBL.h"
#include <cstring>

namespace eyelib {
namespace render {

DeferredLightingPassExecuteParams DeferredLightingPassSystem::BuildParams(const DeferredLightingPassBuildParams& params) const {
    DeferredLightingPassExecuteParams out{};
    out.build = params;

    if (params.forwardFrameUBO) {
        const math::Mat4 viewProj = params.camView.projection * params.camView.view;
        float frameUbo[5 * 16];
        std::memcpy(frameUbo, &params.camView.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 16, &params.camView.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 48, &params.prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &params.prevProjection[0][0], 16 * sizeof(float));
        params.forwardFrameUBO->Update(frameUbo, 0, 320);
    }

    rhi::IDescriptorSet* const passSet = params.deferredLightingPassSet;
    GBuffer* const gbuffer = params.gbuffer;
    LightingPass* const lightingPass = params.lightingPass;
    if (!passSet || !gbuffer || !lightingPass)
        return out;

    passSet->UpdateTexture(0, gbuffer->GetPositionTextureRHI());
    passSet->UpdateTexture(1, gbuffer->GetNormalTextureRHI());
    passSet->UpdateTexture(2, gbuffer->GetAlbedoTextureRHI());
    passSet->UpdateTexture(3, gbuffer->GetPBRParamsTextureRHI());
    if (params.cascadeForLighting)
        passSet->UpdateTexture(4, params.cascadeForLighting->GetDepthTextureRHI());
    if (params.sceneLightingBuffer)
        passSet->UpdateStorageBuffer(5, params.sceneLightingBuffer, 0, 0);
    if (params.cascadeForLighting) {
        rhi::IBuffer* lightSpaceBuf = params.cascadeForLighting->GetLightSpaceBufferRHI();
        if (lightSpaceBuf)
            passSet->UpdateUniformBuffer(6, lightSpaceBuf, 0, 0);
    }
    if (params.ibl && params.ibl->IsValid()) {
        if (rhi::ITexture* ir = params.ibl->GetIrradianceTextureRHI()) passSet->UpdateTexture(7, ir);
        if (rhi::ITexture* pf = params.ibl->GetPrefilterTextureRHI()) passSet->UpdateTexture(8, pf);
        if (rhi::ITexture* brdf = params.ibl->GetBRDFLUTTextureRHI()) passSet->UpdateTexture(9, brdf);
    }
    lightingPass->SetResourceSpaceSets(params.forwardFrameSet, passSet);
    return out;
}

bool DeferredLightingPassSystem::Execute(const DeferredLightingPassExecuteParams& params) const {
    rhi::IDevice* const device = params.build.device;
    rhi::ICommandList* const cmd = params.build.cmd;
    LightingPass* const lightingPass = params.build.lightingPass;
    GBuffer* const gbuffer = params.build.gbuffer;
    if (!params.build.frameData || !lightingPass || !gbuffer) return false;
    if (cmd && params.build.hdrRenderTarget)
        rhi::ClearAndSetRenderTargetWithViewport(cmd, params.build.hdrRenderTarget, 0, 0, params.build.width, params.build.height,
                                                 params.build.clearColor);
    RenderContext c{};
    c.device = device;
    c.cmd = cmd;
    lightingPass->Render(*gbuffer, c, params.build.lightParams, params.build.camView.view, params.build.camView.projection,
                         params.build.camView.viewPos, params.build.cascadeForLighting,
                         &params.build.frameData->pointLights, &params.build.frameData->spotLights,
                         params.build.sceneLightingBuffer, params.build.ibl);
    debug::Profiler::AddFrameCounter("drawCalls", 1);
    debug::Profiler::AddFrameCounter("triangles", 2);
    debug::Profiler::AddFrameCounter("stateChanges", 1);
    if (cmd) cmd->SetRenderTarget(nullptr);
    return true;
}

}  // namespace render
}  // namespace eyelib

