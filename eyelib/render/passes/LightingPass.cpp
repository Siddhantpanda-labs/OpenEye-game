#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/FullscreenQuad.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/resources/Shader.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

LightingPass::LightingPass() = default;

void LightingPass::EnsureDefaultShader() {
    if (m_lightingShader && m_lightingShader->IsValid()) return;
    m_lightingShader = std::make_shared<Shader>();
    m_lightingShader->LoadFromFile("assets/shaders/passes/deferred/lighting.vert", "assets/shaders/passes/deferred/lighting.frag");
}

void LightingPass::SetLightingShader(std::shared_ptr<Shader> shader) {
    m_lightingShader = std::move(shader);
}

void LightingPass::Render(GBuffer& gbuffer,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          ShadowMap* shadowMap) {
    (void)view;
    (void)projection;
    EnsureDefaultShader();
    if (!m_lightingShader->IsValid()) return;

    gbuffer.BindForLightingPass();
    m_lightingShader->Bind();
    if (shadowMap) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, shadowMap->GetDepthTexture());
        m_lightingShader->SetMat4("u_LightSpaceMatrix", shadowMap->GetShadowMatrix());
        m_lightingShader->SetFloat("u_ShadowBias", 0.005f);
    }
    m_lightingShader->SetInt("u_ShadowMap", 5);
    m_lightingShader->SetInt("u_UseShadows", shadowMap ? 1 : 0);
    m_lightingShader->SetVec3("u_ViewPos", viewPos);
    m_lightingShader->SetVec3("u_LightDir", light.direction);
    m_lightingShader->SetVec3("u_LightColor", light.color);
    m_lightingShader->SetFloat("u_LightIntensity", light.intensity);
    m_lightingShader->SetVec3("u_Ambient", light.ambient);
    m_lightingShader->SetFloat("u_Exposure", light.exposure);

    glDisable(GL_DEPTH_TEST);
    static FullscreenQuad quad;
    quad.Render();
    glEnable(GL_DEPTH_TEST);

    if (shadowMap) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    m_lightingShader->Unbind();
}

} // namespace render
} // namespace eyelib
