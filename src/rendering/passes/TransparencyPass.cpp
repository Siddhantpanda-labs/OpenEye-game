#include "TransparencyPass.h"
#include "../../ecs/Components.h"
#include "../../ecs/Systems.h"
#include "../resources/Mesh.h"
#include "../resources/Material.h"
#include "../../core/log/Log.h"
#include <glad/glad.h>
#include <algorithm>
#include <vector>

namespace OpenEye {
namespace Rendering {

void TransparencyPass::Execute(ECS::Scene& scene, const Camera& camera, const Frustum& frustum) {
    auto& registry = scene.GetRegistry();
    
    // Collect transparent renderables with their distances
    struct TransparentEntity {
        ECS::Entity entity;
        float distanceToCamera;
    };
    
    std::vector<TransparentEntity> transparentEntities;
    glm::vec3 cameraPos = camera.GetPosition();
    
    auto view = registry.view<ECS::Transform, ECS::Renderable>();
    for (auto entity : view) {
        auto& renderable = view.get<ECS::Renderable>(entity);
        // Check if transparent (material alpha < 1.0 or explicit flag)
        bool isTransparent = renderable.material && renderable.material->GetAlpha() < 0.99f;
        if (!isTransparent || !renderable.mesh || !renderable.material) {
            continue;
        }
        
        // Check if visible (optional: can add frustum culling here too)
        auto* aabb = registry.try_get<ECS::AABB>(entity);
        if (aabb && !aabb->isVisible) {
            continue; // Culled
        }
        
        // Calculate distance to camera for sorting
        auto& transform = view.get<ECS::Transform>(entity);
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
        float distance = glm::length(cameraPos - worldPos);
        
        transparentEntities.push_back({entity, distance});
    }
    
    // Sort back-to-front (furthest first)
    std::sort(transparentEntities.begin(), transparentEntities.end(),
        [](const TransparentEntity& a, const TransparentEntity& b) {
            return a.distanceToCamera > b.distanceToCamera;
        });
    
    // Enable alpha blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth buffer (transparency order matters)
    
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    glm::mat4 projMatrix = camera.GetProjectionMatrix();
    
    // Render each transparent entity
    for (const auto& transEntity : transparentEntities) {
        auto& renderable = registry.get<ECS::Renderable>(transEntity.entity);
        auto& transform = registry.get<ECS::Transform>(transEntity.entity);
        
        if (!renderable.mesh || !renderable.material || !renderable.material->GetShader()) {
            continue;
        }
        
        glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
        
        // Bind material and shader
        renderable.material->Bind();
        auto shader = renderable.material->GetShader();
        
        // Set matrices (use uniform matrices for forward rendering)
        shader->SetMat4("u_Model", modelMatrix);
        shader->SetMat4("u_View", viewMatrix);
        shader->SetMat4("u_Projection", projMatrix);
        shader->SetVec3("u_ViewPos", cameraPos);
        
        // Set alpha if shader supports it
        float alpha = renderable.material->GetAlpha();
        if (shader->GetUniformInfo("u_Alpha")) {
            shader->SetFloat("u_Alpha", alpha);
        }
        
        // Render mesh
        renderable.mesh->Bind();
        renderable.mesh->Draw();
        renderable.mesh->Unbind();
        
        shader->Unbind();
    }
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace Rendering
} // namespace OpenEye

