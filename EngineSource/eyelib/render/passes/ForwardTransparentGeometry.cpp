#include "eyelib/render/passes/ForwardGeometryCommon.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/passes/common/ForwardSharedEnvironment.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/PassResourceBuilders.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/core/Log.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/math/TransformUtils.h"

#include <algorithm>
#include <cmath>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;

float DistanceToCameraTransparent(const math::Mat4& transform, const math::Vec3& viewPos) {
    math::Vec3 p(transform[3][0], transform[3][1], transform[3][2]);
    math::Vec3 d(p.x - viewPos.x, p.y - viewPos.y, p.z - viewPos.z);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

void ApplyMaterialBlendCullTransparent(rhi::ICommandList* cmd, const Material* material) {
    if (!cmd || !material) return;
    cmd->SetCullFace(!material->GetTwoSided());
    switch (material->GetBlending()) {
    case Blending::Opaque:
        cmd->SetBlend(false);
        cmd->SetDepthWrite(true);
        break;
    case Blending::Translucent:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Additive:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::One, rhi::BlendFactor::One);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Modulate:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::DstColor, rhi::BlendFactor::Zero);
        cmd->SetDepthWrite(false);
        break;
    }
}
}  // namespace

void ForwardTransparentRender(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                              ForwardTransparentRenderContext& ctx) {
    if (!cmd || !ctx.shaderCache || !ctx.ensureIbl || !ctx.buildForwardPassSharedState) return;
    const bool useTypedForwardPush = backend::UseTypedPushConstants(ctx.device);
    IBL* iblPtr = ctx.ensureIbl(frameData);
    const passes::common::ForwardSharedEnvironmentState envState =
        passes::common::ResolveForwardSharedEnvironmentForPipeline(frameData, cam, ctx.cascadeShadowMap, ctx.reflectionProbeManager, iblPtr);
    if (ctx.ensureResourceSets) ctx.ensureResourceSets();
    resources::PassResourceBuilder* passResourceBuilder =
        ctx.getPassResourceBuilder ? ctx.getPassResourceBuilder() : ctx.passResourceBuilder;
    rhi::IDescriptorSet* frameSetForCall = ctx.getFrameSet ? ctx.getFrameSet() : ctx.frameSet;
    rhi::IDescriptorSet* materialSetForCall = ctx.getMaterialSet ? ctx.getMaterialSet() : ctx.materialSet;
    rhi::IBuffer* frameUboForCall = ctx.getFrameUbo ? ctx.getFrameUbo() : ctx.frameUbo;
    rhi::IDescriptorSet* passSetForCall = ctx.acquirePassDrawSet ? ctx.acquirePassDrawSet() : nullptr;
    rhi::IDescriptorSet* sceneSetForCall = ctx.acquireSceneDrawSet ? ctx.acquireSceneDrawSet() : nullptr;
    rhi::IDescriptorSet* bindlessSet = ctx.bindlessHeap ? ctx.bindlessHeap->GetDescriptorSet() : nullptr;
    if (!ForwardPbrResourceBindingsReady(ctx.device, cmd, frameSetForCall, passSetForCall, materialSetForCall,
            sceneSetForCall, frameUboForCall, ctx.bindlessHeap)) {
        static bool s_loggedTrSkip = false;
        if (!s_loggedTrSkip) {
            core::Log::WarnF("RenderForwardTransparent: skipped (need bindless + forward resource-space sets).");
            s_loggedTrSkip = true;
        }
        return;
    }

    struct TEntry {
        size_t idx;
        float dist;
    };
    std::vector<TEntry> sorted;
    sorted.reserve(frameData.visibleTransparent.size());
    for (size_t i = 0; i < frameData.visibleTransparent.size(); ++i) {
        const Drawable& d = frameData.visibleTransparent[i];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) continue;
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        if (material->GetBlending() == Blending::Opaque && alpha >= 0.99f) continue;
        sorted.push_back({ i, DistanceToCameraTransparent(d.transform, cam.viewPos) });
    }
    std::sort(sorted.begin(), sorted.end(), [](const TEntry& a, const TEntry& b) { return a.dist > b.dist; });
    if (sorted.empty()) return;

    ctx.buildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, true);

    const int useIBL = (envState.irradianceTexture || envState.prefilterTexture || envState.brdfLutTexture) ? 1 : 0;

    for (const TEntry& e : sorted) {
        const Drawable& d = frameData.visibleTransparent[e.idx];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) continue;
        ApplyMaterialBlendCullTransparent(cmd, material.get());
        const Material* baseMat = material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ ctx.transparentShaderStage, it, baseMat->GetMaterialShaderId() };
        auto shader = ctx.shaderCache->getOrCompile(ctx.device, key, baseMat);
        if (!shader || !shader->IsValid()) continue;
        rhi::IPipeline* pipe = shader->GetPipelineRHI();
        if (pipe && !pipe->HasValidProgram()) continue;
        Shader* sh = shader.get();
        sh->Bind(ctx.device);
        if (d.materialInstance) d.materialInstance->ApplyToShader(ctx.device, sh, cmd);
        else material->ApplyToShader(ctx.device, sh, cmd);

        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        if ((!bonePtr || boneCount == 0) && d.materialInstance) {
            d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        } else if (!bonePtr || boneCount == 0) {
            d.material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        }
        if (bonePtr && boneCount > 0) boneCount = std::min(boneCount, kMaxBones);

        rhi::IDescriptorSet* drawMaterialSet = ctx.acquireMaterialDrawSet ? ctx.acquireMaterialDrawSet() : nullptr;
        if (!drawMaterialSet) {
            sh->Unbind();
            continue;
        }
        if (passResourceBuilder)
            passResourceBuilder->UpdateMaterialSetWithDefaults(ctx.device, drawMaterialSet, d.material.get(), d.materialInstance.get());
        const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(d.material.get(), d.materialInstance.get());
        rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
        if (bonePtr && boneCount > 0 && ctx.bonesBuffer) {
            const uint32_t boneBytes = static_cast<uint32_t>(boneCount * sizeof(math::Mat4));
            ctx.bonesBuffer->Update(bonePtr, 0, boneBytes);
            rhi::IDescriptorSet* drawSceneSet = ctx.acquireSceneDrawSet ? ctx.acquireSceneDrawSet() : nullptr;
            if (drawSceneSet) {
                drawSceneSet->UpdateStorageBuffer(0, ctx.sceneLightingBuffer, 0, 0);
                drawSceneSet->UpdateStorageBuffer(1, ctx.bonesBuffer, 0, boneBytes);
                drawSceneSet->UpdateStorageBuffer(2, ctx.sceneLightingBuffer, 0, 0);
                rhi::IBuffer* clusterOrDummy = ctx.lightIndexBuffer ? ctx.lightIndexBuffer : ctx.sceneLightingBuffer;
                drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                sceneSetForDraw = drawSceneSet;
            }
        }
        rhi::IDescriptorSet* sets[5] = { frameSetForCall, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
        cmd->BindDescriptorSets(0, 5, sets);
        if (!useTypedForwardPush) {
            sh->SetInt("u_UseIBL", useIBL);
            sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
        }
        if (useTypedForwardPush) {
            const math::Vec4 uvTransform = d.materialInstance
                ? d.materialInstance->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                : (d.material ? d.material->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                              : math::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
            const float uvRotation = d.materialInstance
                ? d.materialInstance->GetFloat("u_UVRotation", 0.0f)
                : (d.material ? d.material->GetFloat("u_UVRotation", 0.0f) : 0.0f);
            const float texCoordIndex = d.materialInstance
                ? d.materialInstance->GetFloat("u_TexCoordIndex", 0.0f)
                : (d.material ? d.material->GetFloat("u_TexCoordIndex", 0.0f) : 0.0f);
            const float flipV = d.materialInstance
                ? d.materialInstance->GetFloat("u_FlipV", 0.0f)
                : (d.material ? d.material->GetFloat("u_FlipV", 0.0f) : 0.0f);
            abi::WriteForwardPushConstants(cmd, d.transform, useIBL, useAlbedoTexture,
                                           uvTransform, uvRotation, texCoordIndex, flipV);
        } else {
            sh->SetMat4("u_Model", d.transform);
            sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        }
        debug::Profiler::AddFrameCounter("drawCalls", 1);
        debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
        d.mesh->Bind(ctx.device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(ctx.device, d.submeshIndex); }
        d.mesh->Unbind();
        sh->Unbind();
    }

    cmd->SetDepthWrite(true);
    cmd->SetBlend(false);
    cmd->SetCullFace(true);
}

}  // namespace render
}  // namespace eyelib

