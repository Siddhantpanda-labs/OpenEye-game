#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;

void applyMaterialBlendAndCull(rhi::ICommandList* cmd, const Material* material) {
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
}

GBufferPass::GBufferPass() = default;

void GBufferPass::SetGeometryShader(std::shared_ptr<Shader> /*shader*/) {
    /* All GBuffer shaders come from ShaderCache + MaterialCompiler; standalone shader is ignored. */
}

std::shared_ptr<Shader> GBufferPass::GetGeometryShader() const {
    return nullptr;
}

void GBufferPass::ReloadShaders() {
    if (m_shaderCache) m_shaderCache->reloadAll();
}

void GBufferPass::Render(GBuffer& gbuffer,
                         const RenderContext& ctx,
                         const std::vector<Drawable>& drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, ctx.cmd, ctx.device,
                    [&drawables](std::function<void(const Drawable&)> c) {
                        for (const auto& d : drawables) c(d);
                    },
                    view, projection, prevView, prevProjection);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         rhi::IDevice* device,
                         rhi::ICommandList* cmd,
                         const std::vector<Drawable>& drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, cmd, device,
                    [&drawables](std::function<void(const Drawable&)> c) {
                        for (const auto& d : drawables) c(d);
                    },
                    view, projection, prevView, prevProjection);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         const RenderContext& ctx,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, ctx.cmd, ctx.device, std::move(feedDrawables), view, projection, prevView, prevProjection);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         rhi::IDevice* device,
                         rhi::ICommandList* cmd,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, cmd, device, std::move(feedDrawables), view, projection, prevView, prevProjection);
}

void GBufferPass::RenderDrawables(GBuffer& gbuffer,
                                  rhi::ICommandList* cmd,
                                  rhi::IDevice* device,
                                  DrawableFeed feedDrawables,
                                  const math::Mat4& view,
                                  const math::Mat4& projection,
                                  const math::Mat4& prevView,
                                  const math::Mat4& prevProjection) {
    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
    }

    const bool useBindless = (m_bindlessHeap && m_bindlessGetHandle && device && device->GetCapabilities().supportsBindless);
    if (useBindless && cmd)
        cmd->BindBindlessHeap(m_bindlessHeap);

    auto drawOne = [this, device, cmd, useBindless](Shader* shader, const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;

        applyMaterialBlendAndCull(cmd, material.get());

        rhi::IBuffer* materialBuffer = d.materialInstance
            ? d.materialInstance->GetMaterialBufferRHI(device)
            : material->GetMaterialBufferRHI(device);
        if (materialBuffer && cmd)
            cmd->BindUniformBuffer(materialBuffer, rhi::UniformBinding::PerMaterial, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));

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
        auto bindTextureToSlot = [&](const std::string& name, int unit) -> bool {
            return d.materialInstance ? d.materialInstance->BindTextureToSlot(name, unit) : material->BindTextureToSlot(name, unit);
        };

        shader->SetMat4("u_Model", d.transform);
        shader->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));

        shader->SetVec4("u_UVTransform", getVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));
        shader->SetFloat("u_UVRotation", getFloat("u_UVRotation", 0.0f));
        shader->SetFloat("u_TexCoordIndex", getFloat("u_TexCoordIndex", 0.0f));
        shader->SetFloat("u_FlipV", getFloat("u_FlipV", 1.0f));

        bool hasBones = false;
        if (d.bones && d.boneCount > 0) {
            const size_t boneCount = std::min(d.boneCount, kMaxBones);
            shader->SetMat4Array("u_Bones", d.bones, boneCount);
            hasBones = true;
        } else if (d.materialInstance) {
            d.materialInstance->ForEachMat4Array([shader, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    const size_t clamped = (name == "u_Bones") ? std::min(count, kMaxBones) : count;
                    shader->SetMat4Array(name, values, clamped);
                    hasBones = true;
                }
            });
        } else {
            material->ForEachMat4Array([shader, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    const size_t clamped = (name == "u_Bones") ? std::min(count, kMaxBones) : count;
                    shader->SetMat4Array(name, values, clamped);
                    hasBones = true;
                }
            });
        }
        if (!hasBones) {
            static const math::Mat4 identityBone = math::Mat4(1.0f);
            shader->SetMat4Array("u_Bones", &identityBone, 1);
        }

        const char* textureOrder[] = {
            "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
            "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
        };
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
        auto hasTex = [&](const std::string& name) {
            auto t = getTexture(name);
            return (t && t->IsValid()) ? 1.0f : 0.0f;
        };
        shader->SetFloat("u_HasAlbedoTexture", hasTex("u_AlbedoTexture"));
        shader->SetFloat("u_HasNormalTexture", hasTex("u_NormalTexture"));
        shader->SetFloat("u_HasMetallicRoughnessTexture", hasTex("u_MetallicRoughnessTexture"));
        shader->SetFloat("u_HasAOTexture", hasTex("u_AOTexture"));
        shader->SetFloat("u_HasMetallicTexture", hasTex("u_MetallicTexture"));
        shader->SetFloat("u_HasRoughnessTexture", hasTex("u_RoughnessTexture"));

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
            shader->SetMat4("u_View", view);
            shader->SetMat4("u_Projection", projection);
            shader->SetMat4("u_PrevView", prevView);
            shader->SetMat4("u_PrevProjection", prevProjection);
            for (const auto& d : drawables)
                drawOne(shader.get(), d);
            shader->Unbind();
        }
    } else {
        /* ShaderCache required; all GBuffer shaders come from MaterialCompiler. */
        gbuffer.Unbind(cmd);
        m_currentGBuffer = nullptr;
        if (cmd) { for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u); cmd->SetCullFace(true); }
    }

    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    if (cmd) {
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
        cmd->SetCullFace(true);
    }
}

void GBufferPass::RenderIndirect(GBuffer& gbuffer,
                                  const RenderContext& ctx,
                                  rhi::IBuffer* indirectBuffer,
                                  uint32_t drawCount,
                                  uint32_t stride,
                                  const Mesh* mesh,
                                  const math::Mat4& view,
                                  const math::Mat4& projection,
                                  const math::Mat4* prevView,
                                  const math::Mat4* prevProjection) {
    RenderIndirect(gbuffer, ctx.device, ctx.cmd, indirectBuffer, drawCount, stride, mesh, view, projection, prevView, prevProjection);
}

void GBufferPass::RenderIndirect(GBuffer& gbuffer,
                                  rhi::IDevice* device,
                                  rhi::ICommandList* cmd,
                                  rhi::IBuffer* indirectBuffer,
                                  uint32_t drawCount,
                                  uint32_t stride,
                                  const Mesh* mesh,
                                  const math::Mat4& view,
                                  const math::Mat4& projection,
                                  const math::Mat4* prevView,
                                  const math::Mat4* prevProjection) {
    if (!device || !cmd || !indirectBuffer || drawCount == 0 || !mesh || !mesh->IsValid() || !m_shaderCache)
        return;
    if (!device->GetCapabilities().supportsMultiDrawIndirect)
        return;

    ShaderKey key{ PipelineStage::GBuffer, InstanceType::Static, kDefaultMaterialTypeId };
    auto shader = m_shaderCache->getOrCompile(device, key);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    shader->Bind(device);
    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        shader->SetMat4("u_PrevView", *prevView);
        shader->SetMat4("u_PrevProjection", *prevProjection);
    }

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
    }

    cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    shader->Unbind();
    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    if (cmd)
        cmd->SetCullFace(true);
}

void GBufferPass::RenderIndirectWithPerDrawData(GBuffer& gbuffer,
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
                                                 const math::Mat4* prevProjection) {
    RenderIndirectWithPerDrawData(gbuffer, ctx.device, ctx.cmd, indirectBuffer, drawCount, stride, mesh, perDrawTransformBuffer, transformCount, view, projection, prevView, prevProjection);
}

void GBufferPass::RenderIndirectWithPerDrawData(GBuffer& gbuffer,
                                                 rhi::IDevice* device,
                                                 rhi::ICommandList* cmd,
                                                 rhi::IBuffer* indirectBuffer,
                                                 uint32_t drawCount,
                                                 uint32_t stride,
                                                 const Mesh* mesh,
                                                 rhi::IBuffer* perDrawTransformBuffer,
                                                 uint32_t transformCount,
                                                 const math::Mat4& view,
                                                 const math::Mat4& projection,
                                                 const math::Mat4* prevView,
                                                 const math::Mat4* prevProjection) {
    if (!device || !cmd || !indirectBuffer || drawCount == 0 || !mesh || !mesh->IsValid()
        || !perDrawTransformBuffer || transformCount == 0 || !m_shaderCache)
        return;
    if (!device->GetCapabilities().supportsMultiDrawIndirect)
        return;

    ShaderKey key{ PipelineStage::GBuffer, InstanceType::Static, kDefaultMaterialTypeId };
    auto shader = m_shaderCache->getOrCompile(device, key);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    shader->Bind(device);
    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        shader->SetMat4("u_PrevView", *prevView);
        shader->SetMat4("u_PrevProjection", *prevProjection);
    }
    shader->SetInt("u_UsePerDrawTransformBuffer", 1);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        cmd->BindStorageBuffer(perDrawTransformBuffer, 4, 0, transformCount * 64u);
    }

    cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    shader->SetInt("u_UsePerDrawTransformBuffer", 0);
    shader->Unbind();
    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    if (cmd)
        cmd->SetCullFace(true);
}

void GBufferPass::RenderInstanced(GBuffer& gbuffer,
                                   rhi::IDevice* device,
                                   rhi::ICommandList* cmd,
                                   const Mesh* mesh,
                                   const Material* material,
                                   rhi::IBuffer* instanceMatrixBuffer,
                                   uint32_t instanceCount,
                                   const math::Mat4& view,
                                   const math::Mat4& projection,
                                   const math::Mat4& prevView,
                                   const math::Mat4& prevProjection,
                                   int32_t submeshIndex) {
    if (!device || !cmd || !mesh || !material || !instanceMatrixBuffer || instanceCount == 0 || !m_shaderCache) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
    }
    if (m_bindlessHeap && m_bindlessGetHandle && device->GetCapabilities().supportsBindless && cmd)
        cmd->BindBindlessHeap(m_bindlessHeap);

    applyMaterialBlendAndCull(cmd, material);

    ShaderKey key{ PipelineStage::GBuffer, InstanceType::Instanced, material->GetShaderIndex() };
    auto shader = m_shaderCache->getOrCompile(device, key, material);
    if (!shader || !shader->IsValid()) {
        gbuffer.Unbind(cmd);
        m_currentGBuffer = nullptr;
        if (cmd) cmd->SetCullFace(true);
        return;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) {
        gbuffer.Unbind(cmd);
        m_currentGBuffer = nullptr;
        if (cmd) cmd->SetCullFace(true);
        return;
    }

    rhi::IBuffer* materialBuffer = material->GetMaterialBufferRHI(device);
    if (materialBuffer) cmd->BindUniformBuffer(materialBuffer, rhi::UniformBinding::PerMaterial, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));

    shader->SetMat4("u_View", view);
    shader->SetMat4("u_Projection", projection);
    shader->SetMat4("u_PrevView", prevView);
    shader->SetMat4("u_PrevProjection", prevProjection);
    shader->SetVec4("u_UVTransform", material->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));
    shader->SetFloat("u_UVRotation", material->GetFloat("u_UVRotation", 0.0f));
    shader->SetFloat("u_TexCoordIndex", material->GetFloat("u_TexCoordIndex", 0.0f));
    shader->SetFloat("u_FlipV", material->GetFloat("u_FlipV", 1.0f));

    const char* textureOrder[] = {
        "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
        "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
    };
    bool useBindless = (m_bindlessHeap && m_bindlessGetHandle && device->GetCapabilities().supportsBindless);
    if (useBindless) {
        for (const char* texName : textureOrder) {
            rhi::ITexture* rhiTex = material->GetTextureRHI(texName);
            rhi::BindlessHandle h = rhiTex && m_bindlessGetHandle ? m_bindlessGetHandle(rhiTex) : rhi::kInvalidBindlessHandle;
            shader->SetHandle64(texName, static_cast<uint64_t>(h));
        }
    } else {
        int unit = 0;
        for (const char* texName : textureOrder) {
            if (material->BindTextureToSlot(texName, unit)) {
                shader->SetInt(texName, unit);
                ++unit;
            }
        }
    }
    auto hasTex = [&](const std::string& name) { return material->GetTexture(name) && material->GetTexture(name)->IsValid() ? 1.0f : 0.0f; };
    shader->SetFloat("u_HasAlbedoTexture", hasTex("u_AlbedoTexture"));
    shader->SetFloat("u_HasNormalTexture", hasTex("u_NormalTexture"));
    shader->SetFloat("u_HasMetallicRoughnessTexture", hasTex("u_MetallicRoughnessTexture"));
    shader->SetFloat("u_HasAOTexture", hasTex("u_AOTexture"));
    shader->SetFloat("u_HasMetallicTexture", hasTex("u_MetallicTexture"));
    shader->SetFloat("u_HasRoughnessTexture", hasTex("u_RoughnessTexture"));

    cmd->BindStorageBuffer(instanceMatrixBuffer, rhi::StorageBinding::InstanceMatrix, 0, instanceCount * 64u);
    shader->Bind(device);
    mesh->Bind(device);
    MeshHandle h = mesh->GetHandle();
    if (h.IsValid()) h.DrawInstanced(device, submeshIndex, instanceCount);
    mesh->Unbind();
    shader->Unbind();

    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    if (cmd) { for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u); cmd->SetCullFace(true); }
}

} // namespace render
} // namespace eyelib
