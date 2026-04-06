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
#include "eyelib/debug/Profiler.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/core/Log.h"

#include <algorithm>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

void ForwardPlusOpaqueRenderBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData, ForwardPlusOpaqueRenderContext& ctx) {
    const CameraView& cam = frameData.camera;

    if (!ctx.shaderCache) return;

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
        static bool s_loggedForwardPlusSkip = false;
        if (!s_loggedForwardPlusSkip) {
            core::Log::WarnF(
                "RenderForwardPlusOpaqueBatched: skipped (need command list, bindless heap, and forward resource-space sets).");
            s_loggedForwardPlusSkip = true;
        }
        return;
    }
    IBL* iblPtr = ctx.ensureIbl ? ctx.ensureIbl(frameData) : nullptr;
    const bool useTypedForwardPush = backend::UseTypedPushConstants(ctx.device);
    const passes::common::ForwardSharedEnvironmentState envState =
        passes::common::ResolveForwardSharedEnvironmentForPipeline(frameData, cam, ctx.cascadeShadowMap, ctx.reflectionProbeManager, iblPtr);
    if (ctx.buildForwardPassSharedState)
        ctx.buildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, true);

    std::vector<DrawBatch> batches = BuildBatches(frameData.visibleOpaque);

    for (DrawBatch& batch : batches) {
        if (batch.IsInstanced() && ctx.instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize && batch.material) {
            ShaderKey key{ PipelineStage::ForwardPlus, InstanceType::Instanced, batch.material->GetMaterialShaderId() };
            auto shader = ctx.shaderCache->getOrCompile(ctx.device, key, batch.material.get());
            if (shader && shader->IsValid()) {
                Shader* sh = shader.get();
                sh->Bind(ctx.device);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
                rhi::IDescriptorSet* drawMaterialSet = ctx.acquireMaterialDrawSet ? ctx.acquireMaterialDrawSet() : nullptr;
                if (!drawMaterialSet) { sh->Unbind(); continue; }
                if (passResourceBuilder)
                    passResourceBuilder->UpdateMaterialSetWithDefaults(ctx.device, drawMaterialSet, batch.material.get(), batch.materialInstance.get());
                const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(batch.material.get(), batch.materialInstance.get());
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                ctx.instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
                rhi::IDescriptorSet* drawSceneSet = ctx.acquireSceneDrawSet ? ctx.acquireSceneDrawSet() : nullptr;
                if (drawSceneSet) {
                    drawSceneSet->UpdateStorageBuffer(0, ctx.sceneLightingBuffer, 0, 0);
                    drawSceneSet->UpdateStorageBuffer(1, ctx.instanceMatrixBuffer, 0, static_cast<uint32_t>(byteSize));
                    drawSceneSet->UpdateStorageBuffer(2, ctx.sceneLightingBuffer, 0, 0);
                    rhi::IBuffer* clusterOrDummy = ctx.lightIndexBuffer ? ctx.lightIndexBuffer : ctx.sceneLightingBuffer;
                    drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                    sceneSetForDraw = drawSceneSet;
                }
                rhi::IDescriptorSet* sets[5] = { frameSetForCall, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
                cmd->BindDescriptorSets(0, 5, sets);
                const int useIBL = (envState.irradianceTexture || envState.prefilterTexture || envState.brdfLutTexture) ? 1 : 0;
                if (useTypedForwardPush) {
                    const math::Vec4 uvTransform = batch.materialInstance
                        ? batch.materialInstance->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                        : (batch.material ? batch.material->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                                          : math::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
                    const float uvRotation = batch.materialInstance
                        ? batch.materialInstance->GetFloat("u_UVRotation", 0.0f)
                        : (batch.material ? batch.material->GetFloat("u_UVRotation", 0.0f) : 0.0f);
                    const float texCoordIndex = batch.materialInstance
                        ? batch.materialInstance->GetFloat("u_TexCoordIndex", 0.0f)
                        : (batch.material ? batch.material->GetFloat("u_TexCoordIndex", 0.0f) : 0.0f);
                    const float flipV = batch.materialInstance
                        ? batch.materialInstance->GetFloat("u_FlipV", 0.0f)
                        : (batch.material ? batch.material->GetFloat("u_FlipV", 0.0f) : 0.0f);
                    // Forward+ instanced VS reads model from instance buffer; keep model identity and provide UV push fields.
                    abi::WriteForwardPushConstants(cmd, math::Mat4(1.0f), useIBL, useAlbedoTexture,
                                                   uvTransform, uvRotation, texCoordIndex, flipV);
                } else {
                    sh->SetInt("u_UseIBL", useIBL);
                }
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
                batch.mesh->Bind(ctx.device);
                { MeshHandle h = batch.mesh->GetHandle(); if (h.IsValid()) h.DrawInstanced(ctx.device, batch.submeshIndex, static_cast<uint32_t>(batch.transforms.size())); }
                batch.mesh->Unbind();
                sh->Unbind();
            }
        } else {
            for (size_t i = 0; i < batch.transforms.size(); ++i) {
                Drawable single = batch.ToDrawable(i);
                const bool useSkinned = (single.bones != nullptr && single.boneCount > 0);
                const Material* baseMat = single.materialInstance ? single.materialInstance->GetMaterial().get() : single.material.get();
                if (!baseMat) continue;
                ShaderKey key{ PipelineStage::ForwardPlus, useSkinned ? InstanceType::Skinned : InstanceType::Static, baseMat->GetMaterialShaderId() };
                auto shader = ctx.shaderCache->getOrCompile(ctx.device, key, baseMat);
                if (!shader || !shader->IsValid()) continue;
                Shader* sh = shader.get();
                sh->Bind(ctx.device);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
                const int useIBL = (envState.irradianceTexture || envState.prefilterTexture || envState.brdfLutTexture) ? 1 : 0;
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseIBL", useIBL);

                rhi::IDescriptorSet* drawMaterialSet = ctx.acquireMaterialDrawSet ? ctx.acquireMaterialDrawSet() : nullptr;
                if (!drawMaterialSet) { sh->Unbind(); continue; }
                if (passResourceBuilder)
                    passResourceBuilder->UpdateMaterialSetWithDefaults(ctx.device, drawMaterialSet, single.material.get(), single.materialInstance.get());
                const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(single.material.get(), single.materialInstance.get());
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
                const math::Mat4* bonePtr = single.bones;
                size_t boneCount = single.boneCount;
                auto collectBones = [&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
                };
                if ((!bonePtr || boneCount == 0) && single.materialInstance) single.materialInstance->ForEachMat4Array(collectBones);
                else if (!bonePtr || boneCount == 0) baseMat->ForEachMat4Array(collectBones);
                rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
                if (bonePtr && boneCount > 0 && ctx.bonesBuffer) {
                    boneCount = std::min(boneCount, kMaxBones);
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
                    const math::Vec4 uvTransform = single.materialInstance
                        ? single.materialInstance->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                        : (single.material ? single.material->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f))
                                           : math::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
                    const float uvRotation = single.materialInstance
                        ? single.materialInstance->GetFloat("u_UVRotation", 0.0f)
                        : (single.material ? single.material->GetFloat("u_UVRotation", 0.0f) : 0.0f);
                    const float texCoordIndex = single.materialInstance
                        ? single.materialInstance->GetFloat("u_TexCoordIndex", 0.0f)
                        : (single.material ? single.material->GetFloat("u_TexCoordIndex", 0.0f) : 0.0f);
                    const float flipV = single.materialInstance
                        ? single.materialInstance->GetFloat("u_FlipV", 0.0f)
                        : (single.material ? single.material->GetFloat("u_FlipV", 0.0f) : 0.0f);
                    abi::WriteForwardPushConstants(cmd, single.transform, useIBL, useAlbedoTexture,
                                                   uvTransform, uvRotation, texCoordIndex, flipV);
                } else {
                    sh->SetMat4("u_Model", single.transform);
                    sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(single.transform)));
                }
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(single.mesh.get(), single.submeshIndex) / 3u);
                single.mesh->Bind(ctx.device);
                { MeshHandle h = single.mesh->GetHandle(); if (h.IsValid()) h.Draw(ctx.device, single.submeshIndex); }
                single.mesh->Unbind();
                sh->Unbind();
            }
        }
    }
}

}  // namespace render
}  // namespace eyelib

