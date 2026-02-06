#include "eyelib/render/passes/LightingPass.h"
#include "api/render/Light.h"
#include "eyelib/render/FullscreenQuad.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/resources/Shader.h"
#include <algorithm>
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

static const int kMaxPointLights = 8;
static const int kMaxSpotLights = 4;

void LightingPass::Render(GBuffer& gbuffer,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          ShadowMap* shadowMap,
                          const std::vector<PointLight>* pointLights,
                          const std::vector<SpotLight>* spotLights) {
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

    const int numPoint = pointLights ? static_cast<int>(std::min(pointLights->size(), size_t(kMaxPointLights))) : 0;
    m_lightingShader->SetInt("u_PointLightCount", numPoint);
    for (int i = 0; i < numPoint; ++i) {
        const auto& p = (*pointLights)[static_cast<size_t>(i)];
        m_lightingShader->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
        m_lightingShader->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
        m_lightingShader->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
        m_lightingShader->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
    }

    const int numSpot = spotLights ? static_cast<int>(std::min(spotLights->size(), size_t(kMaxSpotLights))) : 0;
    m_lightingShader->SetInt("u_SpotLightCount", numSpot);
    for (int i = 0; i < numSpot; ++i) {
        const auto& s = (*spotLights)[static_cast<size_t>(i)];
        m_lightingShader->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
        m_lightingShader->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
        m_lightingShader->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
        m_lightingShader->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
        m_lightingShader->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
        m_lightingShader->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
        m_lightingShader->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
    }

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

void LightingPass::Render(GBuffer& gbuffer,
                          const api::render::DirectionalLight& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          ShadowMap* shadowMap,
                          std::span<const api::render::PointLight> pointLights,
                          std::span<const api::render::SpotLight> spotLights) {
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

    const int numPoint = static_cast<int>(std::min(pointLights.size(), size_t(kMaxPointLights)));
    m_lightingShader->SetInt("u_PointLightCount", numPoint);
    for (int i = 0; i < numPoint; ++i) {
        const auto& p = pointLights[static_cast<size_t>(i)];
        m_lightingShader->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
        m_lightingShader->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
        m_lightingShader->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
        m_lightingShader->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
    }

    const int numSpot = static_cast<int>(std::min(spotLights.size(), size_t(kMaxSpotLights)));
    m_lightingShader->SetInt("u_SpotLightCount", numSpot);
    for (int i = 0; i < numSpot; ++i) {
        const auto& s = spotLights[static_cast<size_t>(i)];
        m_lightingShader->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
        m_lightingShader->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
        m_lightingShader->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
        m_lightingShader->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
        m_lightingShader->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
        m_lightingShader->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
        m_lightingShader->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
    }

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
