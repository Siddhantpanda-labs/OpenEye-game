#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
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

    for (const auto& d : drawables) {
        if (!d.mesh || !d.material) continue;

        m_geometryShader->SetMat4("u_Model", d.transform);
        m_geometryShader->SetVec3("u_Albedo", d.material->GetAlbedo());
        m_geometryShader->SetFloat("u_Metallic", d.material->GetMetallic());
        m_geometryShader->SetFloat("u_Roughness", d.material->GetRoughness());
        m_geometryShader->SetFloat("u_AO", d.material->GetAO());

        // UV params (same as forward skinned)
        m_geometryShader->SetVec4("u_UVTransform", d.material->GetVec4("u_UVTransform", math::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));
        m_geometryShader->SetFloat("u_UVRotation", d.material->GetFloat("u_UVRotation", 0.0f));
        m_geometryShader->SetFloat("u_TexCoordIndex", d.material->GetFloat("u_TexCoordIndex", 0.0f));
        m_geometryShader->SetFloat("u_FlipV", d.material->GetFloat("u_FlipV", 0.0f));

        // Bone matrices for skinning (same as forward: u_Bones UBO). Non-skinned: identity at 0.
        bool hasBones = false;
        d.material->ForEachMat4Array([this, &hasBones](const std::string& name, const math::Mat4* values, size_t count) {
            if (values && count > 0) {
                m_geometryShader->SetMat4Array(name, values, count);
                hasBones = true;
            }
        });
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
            auto tex = d.material->GetTexture(texName);
            if (tex && tex->IsValid()) {
                tex->Bind(unit);
                m_geometryShader->SetInt(texName, unit);
                ++unit;
            }
        }
        m_geometryShader->SetFloat("u_HasAlbedoTexture", d.material->GetTexture("u_AlbedoTexture") && d.material->GetTexture("u_AlbedoTexture")->IsValid() ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasNormalTexture", d.material->GetTexture("u_NormalTexture") && d.material->GetTexture("u_NormalTexture")->IsValid() ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasMetallicRoughnessTexture", d.material->GetTexture("u_MetallicRoughnessTexture") && d.material->GetTexture("u_MetallicRoughnessTexture")->IsValid() ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasAOTexture", d.material->GetTexture("u_AOTexture") && d.material->GetTexture("u_AOTexture")->IsValid() ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasMetallicTexture", d.material->GetTexture("u_MetallicTexture") && d.material->GetTexture("u_MetallicTexture")->IsValid() ? 1.0f : 0.0f);
        m_geometryShader->SetFloat("u_HasRoughnessTexture", d.material->GetTexture("u_RoughnessTexture") && d.material->GetTexture("u_RoughnessTexture")->IsValid() ? 1.0f : 0.0f);

        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    }

    m_geometryShader->Unbind();
    gbuffer.Unbind();
    m_currentGBuffer = nullptr;
}

} // namespace render
} // namespace eyelib
