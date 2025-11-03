#pragma once

#include "../Scene.h"
#include "rendering/Frustum.h"
#include <vector>

namespace OpenEye {
namespace ECS {

// Culling system - performs frustum culling on entities with AABB
class CullingSystem {
public:
    // Update AABBs from mesh bounds (call when mesh changes)
    static void UpdateAABBs(Scene& scene);
    
    // Perform frustum culling, mark entities as visible/invisible
    // Returns count of visible entities
    static size_t CullEntities(Scene& scene, const Rendering::Frustum& frustum);
    
    // Get list of visible entities (for rendering)
    static std::vector<entt::entity> GetVisibleEntities(const entt::registry& registry);
};

} // namespace ECS
} // namespace OpenEye

