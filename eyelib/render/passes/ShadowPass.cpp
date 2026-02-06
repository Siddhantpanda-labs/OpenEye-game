#include "eyelib/render/passes/ShadowPass.h"
#include "api/render/RenderScene.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

ShadowPass::ShadowPass() = default;

void ShadowPass::EnsureDefaultShader() {
    if (m_depthShader && m_depthShader->IsValid()) return;
    m_depthShader = std::make_shared<Shader>();
    m_depthShader->LoadFromFile("assets/shaders/passes/shadow/shadow.vert", "assets/shaders/passes/shadow/shadow.frag");
}

void ShadowPass::EnsureSkinnedShader() {
    if (m_depthSkinnedShader && m_depthSkinnedShader->IsValid()) return;
    m_depthSkinnedShader = std::make_shared<Shader>();
    m_depthSkinnedShader->LoadFromFile("assets/shaders/passes/shadow/shadow_skinned.vert", "assets/shaders/passes/shadow/shadow.frag");
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
    EnsureSkinnedShader();
    if (!m_depthShader->IsValid()) return;

    shadowMap.UpdateMatrices(lightDir, view, projection, cameraPos, nearPlane, farPlane);
    shadowMap.BindForShadowPass();
    m_currentShadowMap = &shadowMap;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glCullFace(GL_FRONT);

    const math::Mat4 lightMatrix = shadowMap.GetShadowMatrix();

    for (const auto& d : drawables) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) continue;

        const math::Mat4* bonePtr = nullptr;
        size_t boneCount = 0;
        if (d.bones && d.boneCount > 0) {
            bonePtr = d.bones;
            boneCount = d.boneCount;
        } else if (d.materialInstance) {
            d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && count > 0) {
                    bonePtr = values;
                    boneCount = count;
                }
            });
        } else {
            material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && count > 0) {
                    bonePtr = values;
                    boneCount = count;
                }
            });
        }

        if (bonePtr && boneCount > 0 && m_depthSkinnedShader->IsValid()) {
            m_depthSkinnedShader->Bind();
            m_depthSkinnedShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthSkinnedShader->SetMat4("u_Model", d.transform);
            m_depthSkinnedShader->SetMat4Array("u_Bones", bonePtr, boneCount);
        } else {
            m_depthShader->Bind();
            m_depthShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthShader->SetMat4("u_Model", d.transform);
        }
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    }

    glCullFace(GL_BACK);
    m_depthShader->Unbind();
    if (m_depthSkinnedShader && m_depthSkinnedShader->IsValid())
        m_depthSkinnedShader->Unbind();
    shadowMap.Unbind();
    m_currentShadowMap = nullptr;
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        std::span<const api::Drawable> drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    if (drawables.empty()) return;
    EnsureDefaultShader();
    EnsureSkinnedShader();
    if (!m_depthShader->IsValid()) return;

    shadowMap.UpdateMatrices(lightDir, view, projection, cameraPos, nearPlane, farPlane);
    shadowMap.BindForShadowPass();
    m_currentShadowMap = &shadowMap;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glCullFace(GL_FRONT);

    const math::Mat4 lightMatrix = shadowMap.GetShadowMatrix();

    for (const auto& apiD : drawables) {
        Mesh* mesh = apiD.mesh ? apiD.mesh->GetInternal().get() : nullptr;
        if (!mesh || !apiD.material) continue;

        const math::Mat4* bonePtr = apiD.bones;
        size_t boneCount = apiD.boneCount;

        if (bonePtr && boneCount > 0 && m_depthSkinnedShader->IsValid()) {
            m_depthSkinnedShader->Bind();
            m_depthSkinnedShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthSkinnedShader->SetMat4("u_Model", apiD.transform);
            m_depthSkinnedShader->SetMat4Array("u_Bones", bonePtr, boneCount);
        } else {
            m_depthShader->Bind();
            m_depthShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthShader->SetMat4("u_Model", apiD.transform);
        }
        mesh->Bind();
        mesh->Draw();
        mesh->Unbind();
    }

    glCullFace(GL_BACK);
    m_depthShader->Unbind();
    if (m_depthSkinnedShader && m_depthSkinnedShader->IsValid())
        m_depthSkinnedShader->Unbind();
    shadowMap.Unbind();
    m_currentShadowMap = nullptr;
}

} // namespace render
} // namespace eyelib
