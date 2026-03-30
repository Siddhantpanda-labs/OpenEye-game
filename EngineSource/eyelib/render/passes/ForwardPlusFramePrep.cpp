#include "eyelib/render/passes/ForwardGeometryCommon.h"
#include "eyelib/render/lighting/ClusteredLightGrid.h"
#include "eyelib/render/passes/detail/LightCullPass.h"
#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/passes/LightCullPassSystem.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"

namespace eyelib {
namespace render {

void ForwardPlusPrepareFrame(rhi::ICommandList* cmd, const RenderFrameData& frameData, ForwardPlusPrepareContext& ctx) {
    if (!ctx.clusteredLightGrid || !ctx.device || !ctx.pointLightBuffer || !ctx.spotLightBuffer || !ctx.lightIndexBuffer) return;

    const CameraView& cam = frameData.camera;
    const LightParams& light = frameData.light;
    const std::vector<PointLight>& pointLights = frameData.pointLights;
    const std::vector<SpotLight>& spotLights = frameData.spotLights;

    if (ctx.clusteredLightGrid->GetNearPlane() != cam.nearPlane ||
        ctx.clusteredLightGrid->GetFarPlane() != cam.farPlane) {
        ctx.clusteredLightGrid->Initialize(ctx.width, ctx.height, cam.nearPlane, cam.farPlane);
    }

    const size_t kMaxPointLights = 256;
    const size_t kMaxSpotLights = 64;
    const size_t requiredLightIndexBytes = ctx.clusteredLightGrid->GetLightIndexDataSize();
    if (!pointLights.empty()) {
        if (!*ctx.pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLights * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            *ctx.pointLightBuffer = ctx.device->CreateBuffer(bd, nullptr);
        }
        if (*ctx.pointLightBuffer) {
            const size_t sz = pointLights.size() * sizeof(PointLight);
            (*ctx.pointLightBuffer)->Update(pointLights.data(), 0, sz);
        }
    }
    if (!spotLights.empty()) {
        if (!*ctx.spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLights * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            *ctx.spotLightBuffer = ctx.device->CreateBuffer(bd, nullptr);
        }
        if (*ctx.spotLightBuffer) {
            const size_t sz = spotLights.size() * sizeof(SpotLight);
            (*ctx.spotLightBuffer)->Update(spotLights.data(), 0, sz);
        }
    }
    if (requiredLightIndexBytes > 0) {
        if (!*ctx.lightIndexBuffer || (*ctx.lightIndexBuffer)->GetSize() < requiredLightIndexBytes) {
            if (*ctx.lightIndexBuffer) {
                ctx.device->DestroyBuffer(*ctx.lightIndexBuffer);
                *ctx.lightIndexBuffer = nullptr;
            }
            rhi::BufferDesc bd;
            bd.sizeInBytes = requiredLightIndexBytes;
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            *ctx.lightIndexBuffer = ctx.device->CreateBuffer(bd, nullptr);
        }
    }

    // Prefer GPU light culling when supported (zero CPU cost); CPU path is fallback.
    if (ctx.lightCullPassSystem && ctx.lightCullPassSystem->IsSupported(ctx.device) && *ctx.lightIndexBuffer) {
        LightCullPass* lightCullPass = ctx.lightCullPassSystem->GetPass();
        lightCullPass->SetPointLights(*ctx.pointLightBuffer, static_cast<uint32_t>(pointLights.size()));
        lightCullPass->SetSpotLights(*ctx.spotLightBuffer, static_cast<uint32_t>(spotLights.size()));
        lightCullPass->SetOutputBuffer(*ctx.lightIndexBuffer);
        lightCullPass->SetViewProj(&cam.view[0][0], &cam.projection[0][0]);
        lightCullPass->SetParams(
            ctx.clusteredLightGrid->GetTileCountX(),
            ctx.clusteredLightGrid->GetTileCountY(),
            ctx.clusteredLightGrid->GetSliceCount(),
            ctx.clusteredLightGrid->GetLogNear(),
            ctx.clusteredLightGrid->GetLogFar(),
            ctx.clusteredLightGrid->GetNearPlane(),
            ctx.clusteredLightGrid->GetFarPlane(),
            ctx.width, ctx.height);
        uint32_t zero = 0u;
        (*ctx.lightIndexBuffer)->Update(&zero, 0, sizeof(zero));
        RenderContext lightCullCtx{};
        lightCullCtx.device = ctx.device;
        lightCullCtx.cmd = cmd;
        lightCullPass->Execute(lightCullCtx);
    } else {
        ctx.clusteredLightGrid->BuildLightGrid(pointLights, spotLights,
                                               cam.view, cam.projection, cam.viewPos);
        ctx.clusteredLightGrid->UploadToGPU(*ctx.lightIndexBuffer);
    }
    if (cmd && ctx.hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, ctx.hdrRenderTarget, 0, 0, ctx.width, ctx.height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    math::Mat4 lightSpaceMatrix(1.0f);
    if (light.castShadows && ctx.cascadeShadowMap && ctx.cascadeShadowMap->GetSplitCount() > 0)
        lightSpaceMatrix = ctx.cascadeShadowMap->GetLightSpaceMatrix(0);
    FillSceneLightingBuffer(ctx.sceneLightingBuffer, light, cam.viewPos, lightSpaceMatrix, 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, ctx.cascadeShadowMap);
}

}  // namespace render
}  // namespace eyelib

