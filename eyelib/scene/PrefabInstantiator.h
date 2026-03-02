#pragma once

#include "Prefab.h"
#include "eyelib/world/EntityId.h"

#include <unordered_map>

namespace eyelib {
namespace scene {

class PrefabInstantiator {
public:
    static Entity Instantiate(Scene& scene, PrefabHandle prefab, const math::Vec3& position, const math::Quat& rotation);
    static Entity Instantiate(Scene& scene, PrefabHandle prefab, Entity parent);
    static void DestroyInstance(Scene& scene, Entity entity);

    static PrefabHandle GetPrefabForInstance(Entity entity);

private:
    static Entity InstantiateNode(Scene& scene, const PrefabNode& node, Entity parent, const math::Transform* overrideRoot);

    static std::unordered_map<Entity, PrefabHandle, world::EntityIdHash> s_instances;
};

} // namespace scene
} // namespace eyelib
