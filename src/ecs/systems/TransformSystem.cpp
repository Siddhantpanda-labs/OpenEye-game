#include "TransformSystem.h"
#include "../Scene.h"
#include "../Components.h"

namespace OpenEye {
namespace ECS {

void TransformSystem::Update(Scene& scene) {
    auto& registry = scene.GetRegistry();
    
    // Update all transforms (GetWorldMatrix handles dirty flag and caching)
    auto view = registry.view<Transform>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        transform.GetWorldMatrix(registry);
    }
}

void TransformSystem::MarkDirty(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity) || !registry.all_of<Transform>(entity)) {
        return;
    }
    
    auto& transform = registry.get<Transform>(entity);
    transform.dirty = true;
    
    // Mark all children as dirty too
    MarkHierarchyDirty(registry, entity);
}

void TransformSystem::MarkHierarchyDirty(entt::registry& registry, entt::entity entity) {
    // Find all children and mark them dirty
    auto view = registry.view<Transform>();
    for (auto child : view) {
        const auto& childTransform = view.get<Transform>(child);
        if (childTransform.parent == entity) {
            auto& mutableTransform = registry.get<Transform>(child);
            mutableTransform.dirty = true;
            MarkHierarchyDirty(registry, child);
        }
    }
}

} // namespace ECS
} // namespace OpenEye

