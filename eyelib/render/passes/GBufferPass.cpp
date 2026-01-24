#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

GBufferPass::GBufferPass() = default;

void GBufferPass::EnsureDefaultShader() {
    if (m_geometryShader && m_geometryShader->IsValid()) return;
    m_geometryShader = std::make_shared<Shader>();
    m_geometryShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", "assets/shaders/passes/deferred/gbuffer.frag");
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
