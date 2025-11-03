#include "CullingSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "rendering/Frustum.h"

namespace OpenEye {
namespace ECS {

void CullingSystem::UpdateAABBs(Scene& scene) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, Renderable, AABB>();
    
    for (auto entity : view) {
        auto [transform, renderable, aabb] = view.get<Transform, Renderable, AABB>(entity);
        
        if (!renderable.mesh || renderable.mesh->GetVertexCount() == 0) {
            continue;
        }
        
        // Get mesh bounds in local space
        glm::vec3 localMin, localMax;
        renderable.mesh->CalculateBounds(localMin, localMax);
        
        // Transform to world space using transform
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        
        // Transform AABB corners to world space
        glm::vec3 corners[8] = {
            glm::vec3(localMin.x, localMin.y, localMin.z),
            glm::vec3(localMax.x, localMin.y, localMin.z),
            glm::vec3(localMin.x, localMax.y, localMin.z),
            glm::vec3(localMax.x, localMax.y, localMin.z),
            glm::vec3(localMin.x, localMin.y, localMax.z),
            glm::vec3(localMax.x, localMin.y, localMax.z),
            glm::vec3(localMin.x, localMax.y, localMax.z),
            glm::vec3(localMax.x, localMax.y, localMax.z)
        };
        
        glm::vec3 worldMin = glm::vec3(worldMatrix * glm::vec4(corners[0], 1.0f));
        glm::vec3 worldMax = worldMin;
        
        for (int i = 1; i < 8; i++) {
            glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corners[i], 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }
        
        aabb.min = worldMin;
        aabb.max = worldMax;
    }
}

size_t CullingSystem::CullEntities(Scene& scene, const Rendering::Frustum& frustum) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, AABB, Renderable>();
    
    size_t visibleCount = 0;
    
    for (auto entity : view) {
        auto [transform, aabb, renderable] = view.get<Transform, AABB, Renderable>(entity);
        
        // Skip if already marked invisible by renderable component
        if (!renderable.visible) {
            aabb.isVisible = false;
            continue;
        }
        
        // Test AABB against frustum
        int result = frustum.TestAABB(aabb.min, aabb.max);
        aabb.isVisible = (result > 0); // 1 = intersect, 2 = inside
        
        if (aabb.isVisible) {
            visibleCount++;
        }
    }
    
    return visibleCount;
}

std::vector<entt::entity> CullingSystem::GetVisibleEntities(const entt::registry& registry) {
    std::vector<entt::entity> visible;
    auto view = registry.view<AABB, Renderable>();
    
    for (auto entity : view) {
        const auto [aabb, renderable] = view.get<AABB, Renderable>(entity);
        if (aabb.isVisible && renderable.visible && renderable.mesh && renderable.material) {
            visible.push_back(entity);
        }
    }
    
    return visible;
}

} // namespace ECS
} // namespace OpenEye

