#pragma once

#include "../Scene.h"
#include "../Components.h"

namespace OpenEye {
namespace ECS {

// Transform system - updates world matrices and handles hierarchy
class TransformSystem {
public:
    static void Update(Scene& scene);
    static void MarkDirty(entt::registry& registry, entt::entity entity);
    static void MarkHierarchyDirty(entt::registry& registry, entt::entity entity);
};

} // namespace ECS
} // namespace OpenEye

