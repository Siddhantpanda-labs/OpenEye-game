#include "eyelib/render/passes/GBufferPassSystem.h"
#include "eyelib/render/passes/detail/GpuCullPass.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/PassResourceBuilders.h"
#include "eyelib/core/Log.h"
#include "eyelib/debug/Profiler.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;

bool GBufferResourceBindingsReady(rhi::IDevice* device, rhi::ICommandList* cmd,
                                  rhi::IDescriptorSet* frameSet, rhi::IDescriptorSet* materialSet,
                                  rhi::IDescriptorSet* sceneSet, rhi::IBindlessHeap* heap) {
    if (!device || !cmd || !frameSet || !materialSet || !sceneSet) return false;
    if (!backend::SupportsBindless(device)) return false;
    if (!heap || !heap->GetDescriptorSet()) return false;
    return true;
}

void ApplyMaterialBlendAndCull(rhi::ICommandList* cmd, const Material* material) {
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
} // namespace

void GBufferPassSystem::SetShaderCache(ShaderCache* cache) {
    m_shaderCache = cache;
}

void GBufferPassSystem::SetBindlessHeap(rhi::IBindlessHeap* heap) {
    m_bindlessHeap = heap;
}

void GBufferPassSystem::SetBindlessGetHandle(std::function<rhi::BindlessHandle(rhi::ITexture*)> fn) {
    m_bindlessGetHandle = std::move(fn);
}

void GBufferPassSystem::SetResourceSpaceSets(rhi::IDescriptorSet* frameSet, rhi::IDescriptorSet* materialSet, rhi::IDescriptorSet* sceneBuffersSet) {
    m_gbufferFrameSet = frameSet;
    m_gbufferMaterialSet = materialSet;
    m_gbufferSceneBuffersSet = sceneBuffersSet;
}

void GBufferPassSystem::SetBonesBuffer(rhi::IBuffer* buffer) {
    m_gbufferBonesBuffer = buffer;
}

GBufferPassSystem::~GBufferPassSystem() {
    Shutdown(m_sceneFallbackDevice);
}

void GBufferPassSystem::Shutdown(rhi::IDevice* device) {
    rhi::IDevice* d = device ? device : m_sceneFallbackDevice;
    if (m_gbufferSceneFallbackBuffer && d) {
        d->DestroyBuffer(m_gbufferSceneFallbackBuffer);
        m_gbufferSceneFallbackBuffer = nullptr;
    }
    m_sceneFallbackDevice = nullptr;
}

void GBufferPassSystem::EnsureSceneBuffersDescriptorWrites(rhi::IDevice* device, rhi::IDescriptorSet* sceneSet) const {
    if (!device || !sceneSet)
        return;

    if (m_sceneFallbackDevice && m_sceneFallbackDevice != device && m_gbufferSceneFallbackBuffer) {
        m_sceneFallbackDevice->DestroyBuffer(m_gbufferSceneFallbackBuffer);
        m_gbufferSceneFallbackBuffer = nullptr;
    }

    if (!m_gbufferSceneFallbackBuffer) {
        rhi::BufferDesc fallbackDesc{};
        fallbackDesc.sizeInBytes = 64u;
        fallbackDesc.usage = rhi::BufferUsage::Storage;
        fallbackDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_gbufferSceneFallbackBuffer = device->CreateBuffer(fallbackDesc, nullptr);
        m_sceneFallbackDevice = device;
    }

    rhi::IBuffer* const fallback = m_gbufferSceneFallbackBuffer;
    if (!fallback)
        return;

    // PassResourceBuilder does not fill SceneBuffers; bind validation requires all four slots.
    rhi::IBuffer* const forBinding1 = m_gbufferBonesBuffer ? m_gbufferBonesBuffer : fallback;
    sceneSet->UpdateStorageBuffer(0, fallback, 0, 0);
    sceneSet->UpdateStorageBuffer(1, forBinding1, 0, m_gbufferBonesBuffer ? static_cast<uint32_t>(kMaxBones * sizeof(math::Mat4)) : 0);
    sceneSet->UpdateStorageBuffer(2, fallback, 0, 0);
    sceneSet->UpdateStorageBuffer(3, fallback, 0, 0);
}

rhi::IDescriptorSet* GBufferPassSystem::AcquireMaterialSetForDraw() const {
    if (m_acquireMaterialDrawSet) {
        if (rhi::IDescriptorSet* set = m_acquireMaterialDrawSet())
            return set;
    }
    return m_gbufferMaterialSet;
}

rhi::IDescriptorSet* GBufferPassSystem::AcquireSceneSetForDraw() const {
    if (m_acquireSceneDrawSet) {
        if (rhi::IDescriptorSet* set = m_acquireSceneDrawSet())
            return set;
    }
    return m_gbufferSceneBuffersSet;
}

void GBufferPassSystem::ReloadShaders() {
    if (m_shaderCache) m_shaderCache->reloadAll();
}

void GBufferPassSystem::RenderDrawablesDirect(GBuffer& gbuffer,
                                              const RenderContext& ctx,
                                              const std::vector<Drawable>& drawables,
                                              const math::Mat4& view,
                                              const math::Mat4& projection,
                                              const math::Mat4& prevView,
                                              const math::Mat4& prevProjection) const {
    RenderDrawables(gbuffer, ctx.cmd, ctx.device,
                    [&drawables](std::function<void(const Drawable&)> c) {
                        for (const auto& d : drawables) c(d);
                    },
                    view, projection, prevView, prevProjection);
}

void GBufferPassSystem::RenderDrawables(GBuffer& gbuffer,
                                        rhi::ICommandList* cmd,
                                        rhi::IDevice* device,
                                        DrawableFeed feedDrawables,
                                        const math::Mat4& view,
                                        const math::Mat4& projection,
                                        const math::Mat4& prevView,
                                        const math::Mat4& prevProjection) const {
    gbuffer.BindForGeometryPass(cmd);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
    }

    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    rhi::IDescriptorSet* initialMaterialSet = AcquireMaterialSetForDraw();
    rhi::IDescriptorSet* initialSceneSet = AcquireSceneSetForDraw();
    if (!GBufferResourceBindingsReady(device, cmd, m_gbufferFrameSet, initialMaterialSet, initialSceneSet, m_bindlessHeap)) {
        static std::atomic<bool> s_warned{ false };
        if (!s_warned.exchange(true))
            core::Log::WarnF("GBufferPassSystem: descriptor sets + bindless not ready; skipping G-buffer geometry.");
        gbuffer.Unbind(cmd);
        if (cmd) {
            for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
            cmd->SetCullFace(true);
        }
        return;
    }

    // Once per pass: complete set 3 so BindDescriptorSets does not reject the SceneBuffers set (static draws never touch bones).
    EnsureSceneBuffersDescriptorWrites(device, initialSceneSet);

    auto drawOne = [this, device, cmd, bindlessSet](Shader* shader, const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;
        rhi::IDescriptorSet* materialSet = AcquireMaterialSetForDraw();
        rhi::IDescriptorSet* sceneSet = AcquireSceneSetForDraw();
        if (!materialSet || !sceneSet) return;
        EnsureSceneBuffersDescriptorWrites(device, sceneSet);

        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        if ((!bonePtr || boneCount == 0) && d.materialInstance) {
            d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        } else if (!bonePtr || boneCount == 0) {
            material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        }
        if (bonePtr && boneCount > 0) boneCount = std::min(boneCount, kMaxBones);

        ApplyMaterialBlendAndCull(cmd, material.get());

        if (d.materialInstance) d.materialInstance->UpdateForwardResourceSet(device, materialSet);
        else material->UpdateForwardResourceSet(device, materialSet);
        // Binding 1 defaults to the shared bones SSBO (or fallback from EnsureSceneBuffersDescriptorWrites); skinned draws upload and narrow the range.
        if (bonePtr && boneCount > 0 && m_gbufferBonesBuffer) {
            const uint32_t boneBytes = static_cast<uint32_t>(boneCount * sizeof(math::Mat4));
            m_gbufferBonesBuffer->Update(bonePtr, 0, boneBytes);
            sceneSet->UpdateStorageBuffer(1, m_gbufferBonesBuffer, 0, boneBytes);
        }
        rhi::IDescriptorSet* sets[4] = { m_gbufferFrameSet, materialSet, bindlessSet, sceneSet };
        cmd->BindDescriptorSets(0, 4, sets);

        auto getVec4 = [&](const std::string& name, const math::Vec4& def) {
            return d.materialInstance ? d.materialInstance->GetVec4(name, def) : material->GetVec4(name, def);
        };
        auto getFloat = [&](const std::string& name, float def) {
            return d.materialInstance ? d.materialInstance->GetFloat(name, def) : material->GetFloat(name, def);
        };
        auto getTexture = [&](const std::string& name) {
            return d.materialInstance ? d.materialInstance->GetTexture(name) : material->GetTexture(name);
        };
        auto getTextureRHI = [&](const std::string& name) -> rhi::ITexture* {
            return d.materialInstance ? d.materialInstance->GetTextureRHI(name) : material->GetTextureRHI(name);
        };
        const math::Vec4 uvT = getVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        const float uvRot = getFloat("u_UVRotation", 0.0f);
        const float texIdx = getFloat("u_TexCoordIndex", 0.0f);
        const float flipV = getFloat("u_FlipV", 1.0f);
        auto hasTex = [&](const std::string& name) {
            auto t = getTexture(name);
            return (t && t->IsValid()) ? 1.0f : 0.0f;
        };
        if (device && backend::IsVulkan(device)) {
            if (cmd) {
                abi::WriteGBufferGeometryPushConstants(cmd, d.transform, uvT, uvRot, texIdx, flipV, hasTex("u_AlbedoTexture"));
                /* Skinned VS reads int at 96; keep 0 for bone path (indirect multi-draw sets 1 elsewhere). */
                const int32_t usePerDrawTransform = 0;
                cmd->SetPushConstants(abi::kGBufferSkinnedUsePerDrawTransformOffset, sizeof(int32_t), &usePerDrawTransform);
            }
        } else {
            shader->SetMat4("u_Model", d.transform);
            shader->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
            shader->SetVec4("u_UVTransform", uvT);
            shader->SetFloat("u_UVRotation", uvRot);
            shader->SetFloat("u_TexCoordIndex", texIdx);
            shader->SetFloat("u_FlipV", flipV);
            const char* textureOrder[] = {
                "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
                "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
            };
            for (const char* texName : textureOrder) {
                rhi::ITexture* rhiTex = getTextureRHI(texName);
                rhi::BindlessHandle h = rhiTex && m_bindlessGetHandle ? m_bindlessGetHandle(rhiTex) : rhi::kInvalidBindlessHandle;
                shader->SetHandle64(texName, static_cast<uint64_t>(h));
            }
            shader->SetFloat("u_HasAlbedoTexture", hasTex("u_AlbedoTexture"));
            shader->SetFloat("u_HasNormalTexture", hasTex("u_NormalTexture"));
            shader->SetFloat("u_HasMetallicRoughnessTexture", hasTex("u_MetallicRoughnessTexture"));
            shader->SetFloat("u_HasAOTexture", hasTex("u_AOTexture"));
            shader->SetFloat("u_HasMetallicTexture", hasTex("u_MetallicTexture"));
            shader->SetFloat("u_HasRoughnessTexture", hasTex("u_RoughnessTexture"));
        }

        d.mesh->Bind(device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device, d.submeshIndex); }
        d.mesh->Unbind();
    };

    if (m_shaderCache) {
        std::map<ShaderKey, std::vector<Drawable>> buckets;
        feedDrawables([&](const Drawable& d) {
            const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
            if (!d.mesh || !material) return;
            InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
            ShaderKey key{ PipelineStage::GBuffer, it, material->GetShaderIndex() };
            buckets[key].push_back(d);
        });
        for (auto& [key, drawables] : buckets) {
            const Material* matForCompile = (key.materialTypeId != kDefaultMaterialTypeId && !drawables.empty())
                ? (drawables[0].materialInstance ? drawables[0].materialInstance->GetMaterial().get() : drawables[0].material.get())
                : nullptr;
            auto shader = matForCompile ? m_shaderCache->getOrCompile(device, key, matForCompile) : m_shaderCache->getOrCompile(device, key);
            if (!shader || !shader->IsValid()) continue;
            rhi::IPipeline* pipe = shader->GetPipelineRHI();
            if (pipe && !pipe->HasValidProgram()) continue;
            shader->Bind(device);
            for (const auto& d : drawables)
                drawOne(shader.get(), d);
            shader->Unbind();
        }
    } else {
        gbuffer.Unbind(cmd);
        if (cmd) { for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u); cmd->SetCullFace(true); }
        return;
    }

    gbuffer.Unbind(cmd);
    if (cmd) {
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
        cmd->SetCullFace(true);
    }
}

void GBufferPassSystem::RenderIndirect(GBuffer& gbuffer,
                                       const RenderContext& ctx,
                                       rhi::IBuffer* indirectBuffer,
                                       uint32_t drawCount,
                                       uint32_t stride,
                                       const Mesh* mesh,
                                       const math::Mat4& view,
                                       const math::Mat4& projection,
                                       const math::Mat4* prevView,
                                       const math::Mat4* prevProjection) const {
    if (!ctx.device || !ctx.cmd || !indirectBuffer || drawCount == 0 || !mesh || !mesh->IsValid() || !m_shaderCache)
        return;
    if (!backend::SupportsMultiDrawIndirect(ctx.device))
        return;

    ShaderKey key{ PipelineStage::GBuffer, InstanceType::Static, kDefaultMaterialTypeId };
    auto shader = m_shaderCache->getOrCompile(ctx.device, key);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    rhi::IDescriptorSet* materialSet = AcquireMaterialSetForDraw();
    rhi::IDescriptorSet* sceneSet = AcquireSceneSetForDraw();
    if (!GBufferResourceBindingsReady(ctx.device, ctx.cmd, m_gbufferFrameSet, materialSet, sceneSet, m_bindlessHeap))
        return;
    EnsureSceneBuffersDescriptorWrites(ctx.device, sceneSet);
    static Material fallbackMaterial;
    resources::UpdateMaterialDescriptorSetWithDefaults(ctx.device, materialSet, &fallbackMaterial, nullptr);

    // G-buffer already cleared and bound by RenderOpaqueGeometry before indirect draw.
    shader->Bind(ctx.device);
    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        shader->SetMat4("u_PrevView", *prevView);
        shader->SetMat4("u_PrevProjection", *prevProjection);
    }

    ctx.cmd->SetDepthTest(true);
    ctx.cmd->SetDepthWrite(true);
    ctx.cmd->SetCullFace(false);
    rhi::IDescriptorSet* sets[4] = { m_gbufferFrameSet, materialSet, bindlessSet, sceneSet };
    ctx.cmd->BindDescriptorSets(0, 4, sets);
    ctx.cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    ctx.cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    ctx.cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    shader->Unbind();
    gbuffer.Unbind(ctx.cmd);
    ctx.cmd->SetCullFace(true);
}

void GBufferPassSystem::RenderIndirectWithPerDrawData(GBuffer& gbuffer,
                                                      const RenderContext& ctx,
                                                      rhi::IBuffer* indirectBuffer,
                                                      uint32_t drawCount,
                                                      uint32_t stride,
                                                      const Mesh* mesh,
                                                      rhi::IBuffer* perDrawTransformBuffer,
                                                      uint32_t transformCount,
                                                      const math::Mat4& view,
                                                      const math::Mat4& projection,
                                                      const math::Mat4* prevView,
                                                      const math::Mat4* prevProjection) const {
    if (!ctx.device || !ctx.cmd || !indirectBuffer || drawCount == 0 || !mesh || !mesh->IsValid()
        || !perDrawTransformBuffer || transformCount == 0 || !m_shaderCache)
        return;
    if (!backend::SupportsMultiDrawIndirect(ctx.device))
        return;

    // Capability gate: only backends that support per-draw transform fetch use the DrawID-aware shader variant.
    const InstanceType shaderInstanceType = backend::SupportsGpuDrivenPerDrawTransformIndirect(ctx.device)
        ? InstanceType::Skinned
        : InstanceType::Static;
    ShaderKey key{ PipelineStage::GBuffer, shaderInstanceType, kDefaultMaterialTypeId };
    auto shader = m_shaderCache->getOrCompile(ctx.device, key);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    rhi::IDescriptorSet* materialSet = AcquireMaterialSetForDraw();
    rhi::IDescriptorSet* sceneSet = AcquireSceneSetForDraw();
    if (!GBufferResourceBindingsReady(ctx.device, ctx.cmd, m_gbufferFrameSet, materialSet, sceneSet, m_bindlessHeap))
        return;
    EnsureSceneBuffersDescriptorWrites(ctx.device, sceneSet);
    static Material fallbackMaterial;
    resources::UpdateMaterialDescriptorSetWithDefaults(ctx.device, materialSet, &fallbackMaterial, nullptr);
    sceneSet->UpdateStorageBuffer(1, perDrawTransformBuffer, 0, transformCount * 64u);

    // G-buffer already cleared and bound by RenderOpaqueGeometry before indirect draw.
    shader->Bind(ctx.device);
    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        shader->SetMat4("u_PrevView", *prevView);
        shader->SetMat4("u_PrevProjection", *prevProjection);
    }
    if (shaderInstanceType == InstanceType::Skinned) {
        shader->SetInt("u_UsePerDrawTransformBuffer", 1);
        if (backend::IsVulkan(ctx.device) && ctx.cmd) {
            const math::Mat4 identity(1.0f);
            const math::Vec4 uvDef(0.0f, 0.0f, 1.0f, 1.0f);
            abi::WriteGBufferGeometryPushConstants(ctx.cmd, identity, uvDef, 0.0f, 0.0f, 1.0f, 0.0f);
            const int32_t usePerDraw = 1;
            ctx.cmd->SetPushConstants(abi::kGBufferSkinnedUsePerDrawTransformOffset, sizeof(int32_t), &usePerDraw);
        }
    }

    ctx.cmd->SetDepthTest(true);
    ctx.cmd->SetDepthWrite(true);
    ctx.cmd->SetCullFace(false);
    rhi::IDescriptorSet* sets[4] = { m_gbufferFrameSet, materialSet, bindlessSet, sceneSet };
    ctx.cmd->BindDescriptorSets(0, 4, sets);
    ctx.cmd->BindStorageBuffer(perDrawTransformBuffer, 4, 0, transformCount * 64u);
    ctx.cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    ctx.cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    ctx.cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    if (shaderInstanceType == InstanceType::Skinned)
        shader->SetInt("u_UsePerDrawTransformBuffer", 0);
    shader->Unbind();
    gbuffer.Unbind(ctx.cmd);
    ctx.cmd->SetCullFace(true);
}

void GBufferPassSystem::RenderInstanced(GBuffer& gbuffer,
                                        rhi::IDevice* device,
                                        rhi::ICommandList* cmd,
                                        const Mesh* mesh,
                                        const Material* material,
                                        const MaterialInstance* materialInstance,
                                        rhi::IBuffer* instanceMatrixBuffer,
                                        uint32_t instanceCount,
                                        const math::Mat4& view,
                                        const math::Mat4& projection,
                                        const math::Mat4& prevView,
                                        const math::Mat4& prevProjection,
                                        int32_t submeshIndex) const {
    if (!device || !cmd || !mesh || !material || !instanceMatrixBuffer || instanceCount == 0 || !m_shaderCache) return;
    const Material* baseMaterial = materialInstance ? materialInstance->GetMaterial().get() : material;
    if (!baseMaterial) return;
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    rhi::IDescriptorSet* materialSet = AcquireMaterialSetForDraw();
    rhi::IDescriptorSet* sceneSet = AcquireSceneSetForDraw();
    if (!GBufferResourceBindingsReady(device, cmd, m_gbufferFrameSet, materialSet, sceneSet, m_bindlessHeap))
        return;
    EnsureSceneBuffersDescriptorWrites(device, sceneSet);

    gbuffer.BindForGeometryPass(cmd);
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);

    ApplyMaterialBlendAndCull(cmd, baseMaterial);

    ShaderKey key{ PipelineStage::GBuffer, InstanceType::Instanced, baseMaterial->GetShaderIndex() };
    auto shader = m_shaderCache->getOrCompile(device, key, baseMaterial);
    if (!shader || !shader->IsValid()) {
        gbuffer.Unbind(cmd);
        cmd->SetCullFace(true);
        return;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) {
        gbuffer.Unbind(cmd);
        cmd->SetCullFace(true);
        return;
    }

    rhi::IBuffer* materialBuffer = materialInstance
        ? materialInstance->GetMaterialBufferRHI(device)
        : baseMaterial->GetMaterialBufferRHI(device);
    if (materialBuffer) cmd->BindUniformBuffer(materialBuffer, rhi::UniformBinding::PerMaterial, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));

    shader->Bind(device);
    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    shader->SetMat4("u_PrevView", prevView);
    shader->SetMat4("u_PrevProjection", prevProjection);
    auto getVec4 = [&](const std::string& name, const math::Vec4& def) {
        return materialInstance ? materialInstance->GetVec4(name, def) : baseMaterial->GetVec4(name, def);
    };
    auto getFloat = [&](const std::string& name, float def) {
        return materialInstance ? materialInstance->GetFloat(name, def) : baseMaterial->GetFloat(name, def);
    };
    auto getTextureRHI = [&](const std::string& name) -> rhi::ITexture* {
        return materialInstance ? materialInstance->GetTextureRHI(name) : baseMaterial->GetTextureRHI(name);
    };
    auto bindTextureToSlot = [&](const std::string& name, int unit) -> bool {
        return materialInstance ? materialInstance->BindTextureToSlot(name, unit) : baseMaterial->BindTextureToSlot(name, unit);
    };
    auto hasTex = [&](const std::string& name) {
        return getTextureRHI(name) ? 1.0f : 0.0f;
    };

    const math::Vec4 uvTInst = getVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    const float uvRotInst = getFloat("u_UVRotation", 0.0f);
    const float texIdxInst = getFloat("u_TexCoordIndex", 0.0f);
    const float flipVInst = getFloat("u_FlipV", 1.0f);
    if (device && backend::IsVulkan(device)) {
        if (cmd)
            abi::WriteGBufferInstancedGeometryPushConstants(cmd, uvTInst, uvRotInst, texIdxInst, flipVInst, hasTex("u_AlbedoTexture"));
    } else {
        shader->SetVec4("u_UVTransform", uvTInst);
        shader->SetFloat("u_UVRotation", uvRotInst);
        shader->SetFloat("u_TexCoordIndex", texIdxInst);
        shader->SetFloat("u_FlipV", flipVInst);
    }

    const char* textureOrder[] = {
        "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
        "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
    };
    bool useBindless = (m_bindlessHeap && m_bindlessGetHandle && backend::SupportsBindless(device));
    if (useBindless) {
        for (const char* texName : textureOrder) {
            rhi::ITexture* rhiTex = getTextureRHI(texName);
            rhi::BindlessHandle h = rhiTex && m_bindlessGetHandle ? m_bindlessGetHandle(rhiTex) : rhi::kInvalidBindlessHandle;
            shader->SetHandle64(texName, static_cast<uint64_t>(h));
        }
    } else {
        int unit = 0;
        for (const char* texName : textureOrder) {
            if (bindTextureToSlot(texName, unit)) {
                shader->SetInt(texName, unit);
                ++unit;
            }
        }
    }
    shader->SetFloat("u_HasAlbedoTexture", hasTex("u_AlbedoTexture"));
    shader->SetFloat("u_HasNormalTexture", hasTex("u_NormalTexture"));
    shader->SetFloat("u_HasMetallicRoughnessTexture", hasTex("u_MetallicRoughnessTexture"));
    shader->SetFloat("u_HasAOTexture", hasTex("u_AOTexture"));
    shader->SetFloat("u_HasMetallicTexture", hasTex("u_MetallicTexture"));
    shader->SetFloat("u_HasRoughnessTexture", hasTex("u_RoughnessTexture"));

    if (materialInstance) materialInstance->UpdateForwardResourceSet(device, materialSet);
    else baseMaterial->UpdateForwardResourceSet(device, materialSet);
    sceneSet->UpdateStorageBuffer(1, instanceMatrixBuffer, 0, instanceCount * 64u);
    rhi::IDescriptorSet* sets[4] = { m_gbufferFrameSet, materialSet, bindlessSet, sceneSet };
    cmd->BindDescriptorSets(0, 4, sets);
    cmd->BindStorageBuffer(instanceMatrixBuffer, rhi::StorageBinding::InstanceMatrix, 0, instanceCount * 64u);
    mesh->Bind(device);
    MeshHandle h = mesh->GetHandle();
    if (h.IsValid()) h.DrawInstanced(device, submeshIndex, instanceCount);
    mesh->Unbind();
    shader->Unbind();

    gbuffer.Unbind(cmd);
    for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
    cmd->SetCullFace(true);
}

void GBufferPassSystem::RenderOpaqueGeometry(const GBufferPassOpaqueParams& p) const {
    if (!p.gbuffer) return;
    RenderContext ctx{};
    ctx.device = p.device;
    ctx.cmd = p.cmd;
    if (!ctx) return;
    if (p.ensureCurrentFrameResourceSets) p.ensureCurrentFrameResourceSets();
    m_acquireMaterialDrawSet = p.acquireMaterialDrawSet;
    m_acquireSceneDrawSet = p.acquireSceneDrawSet;
    if (p.frameUbo) {
        math::Mat4 viewProj = p.cam.projection * p.cam.view;
        float frameUbo[5 * 16];
        std::memcpy(frameUbo, &p.cam.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 16, &p.cam.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 48, &p.prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &p.prevProjection[0][0], 16 * sizeof(float));
        p.frameUbo->Update(frameUbo, 0, 320);
    }

    // Deferred/hybrid contract: G-buffer color + depth must be defined every frame before lighting runs,
    // including when there are zero opaque draws. Otherwise stale depth rejects skybox and deferred re-lights old surfaces.
    p.gbuffer->BindForGeometryPass(p.cmd);

    const bool useIndirectQuad = p.useGpuDrivenGBuffer && p.gpuCullPass && p.gpuCullPass->IsSupported(p.device)
        && p.gpuDrivenQuad && p.gpuDrivenQuad->IsValid() && (p.gpuCullDrawListCount == 0);
    const bool useIndirectScene = p.useGpuDrivenGBuffer && p.gpuCullPass && p.gpuCullPass->IsSupported(p.device)
        && p.gpuCullDrawListCount > 0 && p.gpuCullSingleMesh && p.gpuCullRepresentativeMesh && p.gpuCullRepresentativeMesh->IsValid()
        && p.gpuCullTransformBuffer;
    if (useIndirectQuad) {
        RenderIndirect(*p.gbuffer, ctx,
            p.gpuCullPass->GetIndirectBuffer(),
            p.gpuCullPass->GetDrawCount(),
            p.gpuCullPass->GetIndirectStride(),
            p.gpuDrivenQuad,
            p.cam.view, p.cam.projection, &p.prevView, &p.prevProjection);
        debug::Profiler::AddFrameCounter("drawCalls", 1);
        debug::Profiler::AddFrameCounter("triangles", 2);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else if (useIndirectScene) {
        RenderIndirectWithPerDrawData(*p.gbuffer, ctx,
            p.gpuCullPass->GetIndirectBuffer(),
            p.gpuCullPass->GetDrawCount(),
            p.gpuCullPass->GetIndirectStride(),
            p.gpuCullRepresentativeMesh.get(),
            p.gpuCullTransformBuffer,
            p.gpuCullDrawListCount,
            p.cam.view, p.cam.projection, &p.prevView, &p.prevProjection);
        debug::Profiler::AddFrameCounter("drawCalls", p.gpuCullDrawListCount);
        const uint32_t idxCount = GetIndexCountForDraw(p.gpuCullRepresentativeMesh.get(), -1);
        debug::Profiler::AddFrameCounter("triangles", (idxCount / 3u) * p.gpuCullDrawListCount);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else {
        if (!p.frameData) return;
        std::vector<DrawBatch> batches = BuildBatches(p.frameData->visibleOpaque);
        std::vector<Drawable> singles;
        for (DrawBatch& batch : batches) {
            if (batch.IsInstanced() && p.instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize) {
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                p.instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                RenderInstanced(*p.gbuffer, p.device, p.cmd,
                    batch.mesh.get(), batch.material.get(), batch.materialInstance.get(), p.instanceMatrixBuffer,
                    static_cast<uint32_t>(batch.transforms.size()),
                    p.cam.view, p.cam.projection, p.prevView, p.prevProjection, batch.submeshIndex);
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            } else {
                for (size_t i = 0; i < batch.transforms.size(); ++i)
                    singles.push_back(batch.ToDrawable(i));
            }
        }
        if (!singles.empty()) {
            for (const Drawable& d : singles) {
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            }
            RenderDrawables(*p.gbuffer, ctx.cmd, ctx.device,
                            [&singles](std::function<void(const Drawable&)> c) {
                                for (const auto& d : singles) c(d);
                            },
                            p.cam.view, p.cam.projection, p.prevView, p.prevProjection);
        }
    }
}

}  // namespace render
}  // namespace eyelib
