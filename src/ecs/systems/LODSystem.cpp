#include "LODSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "rendering/LODManager.h"

namespace OpenEye {
namespace ECS {

void LODSystem::Update(Scene& scene, const glm::vec3& cameraPosition) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, LOD, Renderable>();
    
    for (auto entity : view) {
        auto [transform, lod, renderable] = view.get<Transform, LOD, Renderable>(entity);
        
        // Get entity world position
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        glm::vec3 worldPos = glm::vec3(worldMatrix[3]); // Extract position from matrix
        
        // Calculate distance
        float distance = Rendering::LODManager::CalculateDistance(cameraPosition, worldPos);
        
        // Update LOD
        size_t maxLOD = renderable.lodMeshes.empty() ? 0 : renderable.lodMeshes.size();
        size_t newLOD = Rendering::LODManager::UpdateLOD(lod.settings, distance, maxLOD);
        
        // Update active LOD
        renderable.activeLOD = newLOD;
        
        // Update mesh reference if LOD meshes are available
        if (!renderable.lodMeshes.empty() && newLOD < renderable.lodMeshes.size()) {
            renderable.mesh = renderable.lodMeshes[newLOD];
        }
    }
}

} // namespace ECS
} // namespace OpenEye

