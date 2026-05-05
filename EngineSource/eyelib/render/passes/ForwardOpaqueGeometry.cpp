#include "eyelib/render/passes/ForwardGeometryCommon.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/passes/common/ForwardSharedEnvironment.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/PassResourceBuilders.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/core/Log.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/math/TransformUtils.h"

#include <algorithm>
#include <map>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}  // namespace

bool ForwardPbrResourceBindingsReady(rhi::IDevice* device, rhi::ICommandList* cmd, rhi::IDescriptorSet* frameSet,
    rhi::IDescriptorSet* passSet, rhi::IDescriptorSet* materialSet, rhi::IDescriptorSet* sceneBuffersSet,
    rhi::IBuffer* frameUbo, rhi::IBindlessHeap* bindlessHeap) {
    if (!device || !cmd || !frameSet || !passSet || !materialSet || !sceneBuffersSet || !frameUbo || !bindlessHeap)
        return false;
    return bindlessHeap->GetDescriptorSet() != nullptr;
}

void ForwardOpaqueRenderForCamera(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
    const std::vector<Drawable>& opaqueDrawables, ForwardOpaqueRenderContext& ctx) {
    if (!cmd || !ctx.ensureIbl || !ctx.buildForwardPassSharedState) return;
    IBL* iblPtr = ctx.ensureIbl(frameData);
    const bool useTypedForwardPush = backend::UseTypedPushConstants(ctx.device);
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
        static bool s_loggedForwardSkip = false;
        if (!s_loggedForwardSkip) {
            core::Log::WarnF(
                "RenderForwardOpaque: skipped (need command list, bindless heap, and forward resource-space sets). "
                "Ensure supportsBindless, supportsResourceSpaceBindPath, and successful EnsureCurrentFrameResourceSets.");
            s_loggedForwardSkip = true;
        }
        return;
    }

    ctx.buildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, false);

    if (!ctx.shaderCache) return;
    std::map<ShaderKey, std::vector<Drawable>> buckets;
    for (const Drawable& d : opaqueDrawables) {
        if (!d.mesh || !d.material) continue;
        const Material* baseMat = d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Forward, it, baseMat->GetMaterialShaderId() };
        buckets[key].push_back(d);
    }
    size_t shaderCompileMiss = 0;
    size_t shaderInvalid = 0;
    size_t pipelineInvalid = 0;
    size_t drawMaterialSetMiss = 0;
    size_t meshHandleInvalid = 0;
    size_t drawSubmitted = 0;

    for (auto& [key, drawables] : buckets) {
        const Material* matForCompile = drawables[0].materialInstance ? drawables[0].materialInstance->GetMaterial().get() : drawables[0].material.get();
        auto shader = ctx.shaderCache->getOrCompile(ctx.device, key, matForCompile);
        if (!shader) {
            ++shaderCompileMiss;
            continue;
        }
        if (!shader->IsValid()) {
            ++shaderInvalid;
            continue;
        }
        rhi::IPipeline* pipe = shader->GetPipelineRHI();
        if (pipe && !pipe->HasValidProgram()) {
            ++pipelineInvalid;
            continue;
        }
        Shader* sh = shader.get();
        sh->Bind(ctx.device);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
        const int useIBL = (envState.irradianceTexture || envState.prefilterTexture || envState.brdfLutTexture) ? 1 : 0;
        if (!useTypedForwardPush)
            sh->SetInt("u_UseIBL", useIBL);
        for (const Drawable& d : drawables) {
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
                ++drawMaterialSetMiss;
                continue;
            }
            if (passResourceBuilder)
                passResourceBuilder->UpdateMaterialSetWithDefaults(ctx.device, drawMaterialSet, d.material.get(), d.materialInstance.get());
            const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(d.material.get(), d.materialInstance.get());
            if (!useTypedForwardPush)
                sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
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
            d.mesh->Bind(ctx.device);
            {
                MeshHandle h = d.mesh->GetHandle();
                if (h.IsValid()) {
                    h.Draw(ctx.device, d.submeshIndex);
                    ++drawSubmitted;
                } else {
                    ++meshHandleInvalid;
                }
            }
            d.mesh->Unbind();
        }
        sh->Unbind();
    }

    const bool hasVisible = !opaqueDrawables.empty();
    const bool suspicious = hasVisible && drawSubmitted == 0;
    if (suspicious) {
        core::Log::WarnF(
            "ForwardOpaqueSubmit: visible=%zu buckets=%zu draws=%zu shaderMiss=%zu shaderInvalid=%zu pipeInvalid=%zu drawSetMiss=%zu meshHandleInvalid=%zu",
            opaqueDrawables.size(), buckets.size(), drawSubmitted,
            shaderCompileMiss, shaderInvalid, pipelineInvalid, drawMaterialSetMiss, meshHandleInvalid);
    }
}

}  // namespace render
}  // namespace eyelib
