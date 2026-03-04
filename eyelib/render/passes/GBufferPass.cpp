#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cstdint>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

GBufferPass::GBufferPass() = default;

void GBufferPass::EnsureDefaultShader(rhi::IDevice* device) {
    if (m_geometryShader && m_geometryShader->IsValid()) return;
    if (!device) return;
    m_geometryShader = std::make_shared<Shader>();
    m_geometryShader->Load(device, "passes/deferred/gbuffer_skinned", Mesh::GetVertexLayoutRHI());
}

void GBufferPass::SetGeometryShader(std::shared_ptr<Shader> shader) {
    m_geometryShader = std::move(shader);
}

void GBufferPass::ReloadShaders() {
    if (m_geometryShader) m_geometryShader->Reload();
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
    EnsureDefaultShader(device);
    if (!m_geometryShader || !m_geometryShader->IsValid()) return;
    rhi::IPipeline* pipe = m_geometryShader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);

    m_geometryShader->Bind(device);
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    m_geometryShader->SetMat4("u_PrevView", prevView);
    m_geometryShader->SetMat4("u_PrevProjection", prevProjection);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        // Clear texture units so materials with no albedo don't sample stale bindings (e.g. previous frame's HDR).
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
    }

    const bool useBindless = (m_bindlessHeap && m_bindlessGetHandle && device && device->GetCapabilities().supportsBindless);
    if (useBindless && cmd)
        cmd->BindBindlessHeap(m_bindlessHeap);

    feedDrawables([this, &gbuffer, cmd, device, view, projection, prevView, prevProjection, useBindless](const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;

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

        m_geometryShader->SetMat4("u_Model", d.transform);
        m_geometryShader->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        m_geometryShader->SetVec3("u_Albedo", d.materialInstance ? d.materialInstance->GetAlbedo() : material->GetAlbedo());
        m_geometryShader->SetFloat("u_Metallic", d.materialInstance ? d.materialInstance->GetMetallic() : material->GetMetallic());
        m_geometryShader->SetFloat("u_Roughness", d.materialInstance ? d.materialInstance->GetRoughness() : material->GetRoughness());
        m_geometryShader->SetFloat("u_AO", d.materialInstance ? d.materialInstance->GetAO() : material->GetAO());

        // UV params (same as forward skinned)
        m_geometryShader->SetVec4("u_UVTransform", getVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));
        m_geometryShader->SetFloat("u_UVRotation", getFloat("u_UVRotation", 0.0f));
        m_geometryShader->SetFloat("u_TexCoordIndex", getFloat("u_TexCoordIndex", 0.0f));
        m_geometryShader->SetFloat("u_FlipV", getFloat("u_FlipV", 0.0f));

        // Bone matrices for skinning (same as forward: u_Bones UBO). Non-skinned: identity at 0.
        bool hasBones = false;
        if (d.bones && d.boneCount > 0) {
            const size_t boneCount = std::min(d.boneCount, kMaxBones);
            m_geometryShader->SetMat4Array("u_Bones", d.bones, boneCount);
            hasBones = true;
        } else if (d.materialInstance) {
            d.materialInstance->ForEachMat4Array([this, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    const size_t clamped = (name == "u_Bones") ? std::min(count, kMaxBones) : count;
                    m_geometryShader->SetMat4Array(name, values, clamped);
                    hasBones = true;
                }
            });
        } else {
            material->ForEachMat4Array([this, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    const size_t clamped = (name == "u_Bones") ? std::min(count, kMaxBones) : count;
                    m_geometryShader->SetMat4Array(name, values, clamped);
                    hasBones = true;
                }
            });
        }
        if (!hasBones) {
            static const math::Mat4 identityBone = math::Mat4(1.0f);
            m_geometryShader->SetMat4Array("u_Bones", &identityBone, 1);
        }

        // Material textures: C2 unified path — bindless (handle) or bind-per-slot
        const char* textureOrder[] = {
            "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
            "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
        };
        if (useBindless) {
            for (const char* texName : textureOrder) {
                rhi::ITexture* rhiTex = getTextureRHI(texName);
                rhi::BindlessHandle h = rhiTex && m_bindlessGetHandle ? m_bindlessGetHandle(rhiTex) : rhi::kInvalidBindlessHandle;
                m_geometryShader->SetHandle64(texName, static_cast<uint64_t>(h));
            }
        } else {
            int unit = 0;
            for (const char* texName : textureOrder) {
                if (bindTextureToSlot(texName, unit)) {
                    m_geometryShader->SetInt(texName, unit);
                    ++unit;
                }
            }
        }
        auto hasTex = [&](const std::string& name) {
            auto t = getTexture(name);
            return (t && t->IsValid()) ? 1.0f : 0.0f;
        };
        m_geometryShader->SetFloat("u_HasAlbedoTexture", hasTex("u_AlbedoTexture"));
        m_geometryShader->SetFloat("u_HasNormalTexture", hasTex("u_NormalTexture"));
        m_geometryShader->SetFloat("u_HasMetallicRoughnessTexture", hasTex("u_MetallicRoughnessTexture"));
        m_geometryShader->SetFloat("u_HasAOTexture", hasTex("u_AOTexture"));
        m_geometryShader->SetFloat("u_HasMetallicTexture", hasTex("u_MetallicTexture"));
        m_geometryShader->SetFloat("u_HasRoughnessTexture", hasTex("u_RoughnessTexture"));

        d.mesh->Bind(device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device); }
        d.mesh->Unbind();
    });

    m_geometryShader->Unbind();
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
    if (!device || !cmd || !indirectBuffer || drawCount == 0 || !mesh || !mesh->IsValid())
        return;
    if (!device->GetCapabilities().supportsMultiDrawIndirect)
        return;

    EnsureDefaultShader(device);
    if (!m_geometryShader || !m_geometryShader->IsValid()) return;
    rhi::IPipeline* pipe = m_geometryShader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    m_geometryShader->Bind(device);
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        m_geometryShader->SetMat4("u_PrevView", *prevView);
        m_geometryShader->SetMat4("u_PrevProjection", *prevProjection);
    }

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
    }

    cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    m_geometryShader->Unbind();
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
        || !perDrawTransformBuffer || transformCount == 0)
        return;
    if (!device->GetCapabilities().supportsMultiDrawIndirect)
        return;

    EnsureDefaultShader(device);
    if (!m_geometryShader || !m_geometryShader->IsValid()) return;
    rhi::IPipeline* pipe = m_geometryShader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    m_geometryShader->Bind(device);
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    if (prevView && prevProjection) {
        m_geometryShader->SetMat4("u_PrevView", *prevView);
        m_geometryShader->SetMat4("u_PrevProjection", *prevProjection);
    }
    m_geometryShader->SetInt("u_UsePerDrawTransformBuffer", 1);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
        cmd->BindStorageBuffer(perDrawTransformBuffer, 4, 0, transformCount * 64u);
    }

    cmd->BindVertexBuffer(mesh->GetVertexBufferRHI(), 0);
    cmd->BindIndexBuffer(mesh->GetIndexBufferRHI(), mesh->GetRHIIndexType());
    cmd->MultiDrawIndexedIndirect(indirectBuffer, 0, drawCount, stride);

    m_geometryShader->SetInt("u_UsePerDrawTransformBuffer", 0);
    m_geometryShader->Unbind();
    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    if (cmd)
        cmd->SetCullFace(true);
}

} // namespace render
} // namespace eyelib
