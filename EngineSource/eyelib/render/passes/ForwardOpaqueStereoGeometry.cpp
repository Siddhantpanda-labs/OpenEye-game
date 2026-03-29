#include "eyelib/render/passes/ForwardGeometryCommon.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/math/Frustum.h"

#include <algorithm>
#include <string>

namespace eyelib {
namespace render {

namespace {
bool IsForwardDrawableVisibleStereo(const Drawable& d, const math::Frustum& frustum) {
    if (!d.mesh) return false;
    // Keep skinned meshes conservative to avoid incorrect culling with animated bounds.
    if (d.bones && d.boneCount > 0) return true;
    const math::AABB worldAabb = d.mesh->GetAABB().Transform(d.transform);
    return frustum.Contains(worldAabb) != 0;
}
}  // namespace

void ForwardOpaqueRenderStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam,
                               ForwardStereoRenderContext& ctx) {
    if (!cmd || !ctx.device || !ctx.renderForwardOpaqueForCamera) return;
    if (backend::PreferVulkanTransparencyPath(ctx.device, ctx.shaderCache)) {
        math::Frustum leftFrustum;
        math::Frustum rightFrustum;
        leftFrustum.FromMatrix(leftCam.projection * leftCam.view);
        rightFrustum.FromMatrix(rightCam.projection * rightCam.view);
        std::vector<Drawable> leftOpaque;
        std::vector<Drawable> rightOpaque;
        leftOpaque.reserve(frameData.allOpaque.size());
        rightOpaque.reserve(frameData.allOpaque.size());
        for (const Drawable& d : frameData.allOpaque) {
            if (!d.mesh || !d.material) continue;
            if (IsForwardDrawableVisibleStereo(d, leftFrustum)) leftOpaque.push_back(d);
            if (IsForwardDrawableVisibleStereo(d, rightFrustum)) rightOpaque.push_back(d);
        }
        const uint32_t halfW = ctx.width / 2;
        cmd->SetViewport(0, 0, halfW, ctx.height);
        ctx.renderForwardOpaqueForCamera(cmd, frameData, leftCam, leftOpaque);
        cmd->SetViewport(halfW, 0, halfW, ctx.height);
        ctx.renderForwardOpaqueForCamera(cmd, frameData, rightCam, rightOpaque);
        return;
    }

    if (ctx.ensureStereoForwardShader) ctx.ensureStereoForwardShader();
    Shader* stereoForwardShader = ctx.getStereoForwardShader ? ctx.getStereoForwardShader() : nullptr;
    if (!stereoForwardShader || !stereoForwardShader->IsValid()) return;
    math::Frustum leftFrustum;
    math::Frustum rightFrustum;
    leftFrustum.FromMatrix(leftCam.projection * leftCam.view);
    rightFrustum.FromMatrix(rightCam.projection * rightCam.view);

    const LightParams& light = frameData.light;
    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    const bool useShadows = (ctx.cascadeShadowMap != nullptr && light.castShadows);
    if (useShadows) {
        rhi::ITexture* shadowTex = ctx.cascadeShadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const uint32_t halfW = ctx.width / 2;
    cmd->SetViewportIndexed(0, 0, 0, halfW, ctx.height);
    cmd->SetViewportIndexed(1, halfW, 0, halfW, ctx.height);

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(frameData.pointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(frameData.spotLights.size(), size_t(kMaxSpotLights)));

    math::Mat4 viewArr[2] = { leftCam.view, rightCam.view };
    math::Mat4 projArr[2] = { leftCam.projection, rightCam.projection };

    FillSceneLightingBuffer(ctx.sceneLightingBuffer, light, leftCam.viewPos,
                            ctx.cascadeShadowMap && ctx.cascadeShadowMap->GetSplitCount() > 0 ? ctx.cascadeShadowMap->GetLightSpaceMatrix(0) : math::Mat4(1.0f),
                            0.005f, light.castShadows, &frameData.pointLights, &frameData.spotLights, ctx.cascadeShadowMap);
    if (ctx.sceneLightingBuffer)
        cmd->BindStorageBuffer(ctx.sceneLightingBuffer, 5, 0, 0);

    for (const Drawable& d : frameData.allOpaque) {
        if (!d.mesh || !d.material) continue;
        if (!IsForwardDrawableVisibleStereo(d, leftFrustum) && !IsForwardDrawableVisibleStereo(d, rightFrustum)) continue;
        Material* mat = (d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get());
        if (!mat) continue;

        stereoForwardShader->Bind(ctx.device);
        Shader* sh = stereoForwardShader;
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4Array("u_View", viewArr, 2);
        sh->SetMat4Array("u_Projection", projArr, 2);
        sh->SetVec3("u_ViewPos[0]", leftCam.viewPos);
        sh->SetVec3("u_ViewPos[1]", rightCam.viewPos);
        sh->SetVec3("u_LightDir", light.direction);
        sh->SetVec3("u_LightColor", light.color);
        sh->SetFloat("u_LightIntensity", light.intensity);
        sh->SetVec3("u_Ambient", light.ambient);
        sh->SetFloat("u_Exposure", light.exposure);
        if (useShadows) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", ctx.cascadeShadowMap->GetSplitCount() > 0 ? ctx.cascadeShadowMap->GetLightSpaceMatrix(0) : math::Mat4(1.0f));
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
        sh->SetVec3("u_Albedo", mat->GetAlbedo());
        sh->SetFloat("u_Metallic", mat->GetMetallic());
        sh->SetFloat("u_Roughness", mat->GetRoughness());
        sh->SetVec3("u_Emission", mat->GetEmission());
        sh->SetFloat("u_Alpha", mat->GetAlpha());
        sh->SetInt("u_PointLightCount", numPoint);
        for (int i = 0; i < numPoint; ++i) {
            const auto& p = frameData.pointLights[static_cast<size_t>(i)];
            sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
            sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
            sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
            sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
        }
        sh->SetInt("u_SpotLightCount", numSpot);
        for (int i = 0; i < numSpot; ++i) {
            const auto& s = frameData.spotLights[static_cast<size_t>(i)];
            sh->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
            sh->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
            sh->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
            sh->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
            sh->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
            sh->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
            sh->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
        }
        d.mesh->Bind(ctx.device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.DrawInstanced(ctx.device, -1, 2); }
        d.mesh->Unbind();
        stereoForwardShader->Unbind();
    }

    if (useShadows) cmd->UnbindTexture(kShadowMapUnit);
}

}  // namespace render
}  // namespace eyelib

