#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/core/Log.h"
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

GBufferPass::GBufferPass() = default;

void GBufferPass::EnsureDefaultShader() {
    if (m_geometryShader && m_geometryShader->IsValid()) return;
    rhi::IDevice* device = rhi::GetDefaultDevice();
    if (!device) return;
    m_geometryShader = std::make_shared<Shader>();
    m_geometryShader->Load(device, "passes/deferred/gbuffer_skinned", Mesh::GetVertexLayoutRHI());
}

void GBufferPass::SetGeometryShader(std::shared_ptr<Shader> shader) {
    m_geometryShader = std::move(shader);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         rhi::ICommandList* cmd,
                         const std::vector<Drawable>& drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, cmd,
                    [&drawables](std::function<void(const Drawable&)> c) {
                        for (const auto& d : drawables) c(d);
                    },
                    view, projection, prevView, prevProjection);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         rhi::ICommandList* cmd,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    RenderDrawables(gbuffer, cmd, std::move(feedDrawables), view, projection, prevView, prevProjection);
}

void GBufferPass::RenderDrawables(GBuffer& gbuffer,
                                  rhi::ICommandList* cmd,
                                  DrawableFeed feedDrawables,
                                  const math::Mat4& view,
                                  const math::Mat4& projection,
                                  const math::Mat4& prevView,
                                  const math::Mat4& prevProjection) {
    EnsureDefaultShader();
    bool shaderValid = m_geometryShader && m_geometryShader->IsValid();
    rhi::IPipeline* pipe = m_geometryShader ? m_geometryShader->GetPipelineRHI() : nullptr;
    bool programValid = (!pipe || pipe->HasValidProgram());
    core::Log::InfoF("GBufferPass: geometry shader valid=%d pipeline program valid=%d", shaderValid ? 1 : 0, programValid ? 1 : 0);
    if (!shaderValid) {
        core::Log::Info("GBufferPass: skipped (geometry shader invalid or missing)");
        return;
    }
    if (pipe && !pipe->HasValidProgram()) {
        core::Log::Info("GBufferPass: skipped (no valid backend program, e.g. BGFX GLSL-only)");
        return;
    }
    core::Log::Info("GBufferPass: executing");

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass(cmd);
    core::Log::Info("GBufferPass: BindForGeometryPass done");

    m_geometryShader->Bind();
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    m_geometryShader->SetMat4("u_PrevView", prevView);
    m_geometryShader->SetMat4("u_PrevProjection", prevProjection);

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetCullFace(false);
    }

    feedDrawables([this, &gbuffer, cmd, view, projection, prevView, prevProjection](const Drawable& d) {
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

        // Material textures: bind in fixed order and set u_Has* for gbuffer_skinned.frag
        const char* textureOrder[] = {
            "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
            "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
        };
        int unit = 0;
        for (const char* texName : textureOrder) {
            auto tex = getTexture(texName);
            if (tex && tex->IsValid()) {
                tex->Bind(unit);
                m_geometryShader->SetInt(texName, unit);
                ++unit;
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

        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    });

    core::Log::Info("GBufferPass: drew, unbinding");
    m_geometryShader->Unbind();
    gbuffer.Unbind(cmd);
    m_currentGBuffer = nullptr;
    // Unbind material texture units so Lighting pass only sees GBuffer bindings
    if (cmd) {
        for (uint32_t u = 0; u <= 7; ++u) cmd->UnbindTexture(u);
        cmd->SetCullFace(true);
    }
    core::Log::Info("GBufferPass: done");
}

} // namespace render
} // namespace eyelib
