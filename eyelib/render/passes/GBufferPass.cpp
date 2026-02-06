#include "eyelib/render/passes/GBufferPass.h"
#include "api/render/RenderScene.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

GBufferPass::GBufferPass() = default;

void GBufferPass::EnsureDefaultShader() {
    if (m_geometryShader && m_geometryShader->IsValid()) return;
    m_geometryShader = std::make_shared<Shader>();
    m_geometryShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer_skinned.vert", "assets/shaders/passes/deferred/gbuffer_skinned.frag");
}

void GBufferPass::SetGeometryShader(std::shared_ptr<Shader> shader) {
    m_geometryShader = std::move(shader);
}

void GBufferPass::Render(GBuffer& gbuffer,
                         const std::vector<Drawable>& drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    if (drawables.empty()) return;
    EnsureDefaultShader();
    if (!m_geometryShader->IsValid()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass();

    m_geometryShader->Bind();
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    m_geometryShader->SetMat4("u_PrevView", prevView);
    m_geometryShader->SetMat4("u_PrevProjection", prevProjection);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);

    for (const auto& d : drawables) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) continue;

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
            m_geometryShader->SetMat4Array("u_Bones", d.bones, d.boneCount);
            hasBones = true;
        } else if (d.materialInstance) {
            d.materialInstance->ForEachMat4Array([this, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    m_geometryShader->SetMat4Array(name, values, count);
                    hasBones = true;
                }
            });
        } else {
            material->ForEachMat4Array([this, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
                if (values && count > 0) {
                    m_geometryShader->SetMat4Array(name, values, count);
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
    }

    m_geometryShader->Unbind();
    gbuffer.Unbind();
    m_currentGBuffer = nullptr;
    if (wasCullEnabled) {
        glEnable(GL_CULL_FACE);
    }
}

void GBufferPass::Render(GBuffer& gbuffer,
                         std::span<const api::Drawable> drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    if (drawables.empty()) return;
    EnsureDefaultShader();
    if (!m_geometryShader->IsValid()) return;

    m_currentGBuffer = &gbuffer;
    gbuffer.BindForGeometryPass();

    m_geometryShader->Bind();
    m_geometryShader->SetMat4("u_View", view);
    m_geometryShader->SetMat4("u_Projection", projection);
    m_geometryShader->SetMat4("u_PrevView", prevView);
    m_geometryShader->SetMat4("u_PrevProjection", prevProjection);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);

    for (const auto& apiD : drawables) {
        Mesh* mesh = apiD.mesh ? apiD.mesh->GetInternal().get() : nullptr;
        Material* material = apiD.material ? apiD.material->GetInternal().get() : nullptr;
        if (!mesh || !material) continue;

        const auto& p = apiD.params;
        m_geometryShader->SetMat4("u_Model", apiD.transform);
        m_geometryShader->SetVec3("u_Albedo", p.albedo);
        m_geometryShader->SetFloat("u_Metallic", p.metallic);
        m_geometryShader->SetFloat("u_Roughness", p.roughness);
        m_geometryShader->SetFloat("u_AO", p.ao);
        m_geometryShader->SetVec4("u_UVTransform", p.uvTransform);
        m_geometryShader->SetFloat("u_UVRotation", p.uvRotation);
        m_geometryShader->SetFloat("u_TexCoordIndex", p.texCoordIndex);
        m_geometryShader->SetFloat("u_FlipV", p.flipV);

        bool hasBones = false;
        if (apiD.bones && apiD.boneCount > 0) {
            m_geometryShader->SetMat4Array("u_Bones", apiD.bones, apiD.boneCount);
            hasBones = true;
        }
        if (!hasBones) {
            static const math::Mat4 identityBone = math::Mat4(1.0f);
            m_geometryShader->SetMat4Array("u_Bones", &identityBone, 1);
        }

        const std::shared_ptr<api::Texture>* texs[] = {
            &p.albedoTexture, &p.normalTexture, &p.metallicRoughnessTexture,
            &p.aoTexture, &p.emissiveTexture, &p.metallicTexture, &p.roughnessTexture
        };
        const char* textureOrder[] = {
            "u_AlbedoTexture", "u_NormalTexture", "u_MetallicRoughnessTexture",
            "u_AOTexture", "u_EmissiveTexture", "u_MetallicTexture", "u_RoughnessTexture"
        };
        int unit = 0;
        for (size_t i = 0; i < 7; ++i) {
            const auto& tex = *texs[i];
            if (tex && tex->IsValid()) {
                tex->GetInternal()->Bind(static_cast<int>(unit));
                m_geometryShader->SetInt(textureOrder[i], static_cast<int>(unit));
                ++unit;
            }
        }
        m_geometryShader->SetFloat("u_HasAlbedoTexture", (p.albedoTexture && p.albedoTexture->IsValid()) ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasNormalTexture", (p.normalTexture && p.normalTexture->IsValid()) ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasMetallicRoughnessTexture", (p.metallicRoughnessTexture && p.metallicRoughnessTexture->IsValid()) ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasAOTexture", (p.aoTexture && p.aoTexture->IsValid()) ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasMetallicTexture", (p.metallicTexture && p.metallicTexture->IsValid()) ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasRoughnessTexture", (p.roughnessTexture && p.roughnessTexture->IsValid()) ? 1.0f : 0.0f);

        mesh->Bind();
        mesh->Draw();
        mesh->Unbind();
    }

    m_geometryShader->Unbind();
    gbuffer.Unbind();
    m_currentGBuffer = nullptr;
    if (wasCullEnabled) {
        glEnable(GL_CULL_FACE);
    }
}

} // namespace render
} // namespace eyelib
