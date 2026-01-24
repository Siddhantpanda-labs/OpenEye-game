#include "eyelib/render/passes/ShadowPass.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

ShadowPass::ShadowPass() = default;

void ShadowPass::EnsureDefaultShader() {
    if (m_depthShader && m_depthShader->IsValid()) return;
    m_depthShader = std::make_shared<Shader>();
    m_depthShader->LoadFromFile("assets/shaders/passes/shadow/shadow.vert", "assets/shaders/passes/shadow/shadow.frag");
}

void ShadowPass::SetDepthShader(std::shared_ptr<Shader> shader) {
    m_depthShader = std::move(shader);
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        const std::vector<Drawable>& drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    if (drawables.empty()) return;
    EnsureDefaultShader();
    if (!m_depthShader->IsValid()) return;

    shadowMap.UpdateMatrices(lightDir, view, projection, cameraPos, nearPlane, farPlane);
    shadowMap.BindForShadowPass();
    m_currentShadowMap = &shadowMap;

    m_depthShader->Bind();
    m_depthShader->SetMat4("u_LightSpaceMatrix", shadowMap.GetShadowMatrix());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glCullFace(GL_FRONT);

    for (const auto& d : drawables) {
        if (!d.mesh || !d.material) continue;
        m_depthShader->SetMat4("u_Model", d.transform);
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    }

    glCullFace(GL_BACK);
    m_depthShader->Unbind();
    shadowMap.Unbind();
    m_currentShadowMap = nullptr;
}

} // namespace render
} // namespace eyelib
