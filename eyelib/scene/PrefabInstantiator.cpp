#include "PrefabInstantiator.h"

#include "eyelib/ecs/components/All.h"
#include "entt/entt.hpp"

namespace eyelib {
namespace scene {

std::unordered_map<Entity, PrefabHandle> PrefabInstantiator::s_instances{};

Entity PrefabInstantiator::Instantiate(Scene& scene, PrefabHandle prefab,
                                       const math::Vec3& position, const math::Quat& rotation) {
    const PrefabNode* root = Prefab::GetRoot(prefab);
    if (!root) return NullEntity;
    math::Transform override;
    override.position = position;
    override.rotation = rotation;
    override.scale = root->transform.scale;
    Entity entity = InstantiateNode(scene, *root, NullEntity, &override);
    if (entity != NullEntity) s_instances[entity] = prefab;
    return entity;
}

Entity PrefabInstantiator::Instantiate(Scene& scene, PrefabHandle prefab, Entity parent) {
    const PrefabNode* root = Prefab::GetRoot(prefab);
    if (!root) return NullEntity;
    Entity entity = InstantiateNode(scene, *root, parent, nullptr);
    if (entity != NullEntity) s_instances[entity] = prefab;
    return entity;
}

void PrefabInstantiator::DestroyInstance(Scene& scene, Entity entity) {
    s_instances.erase(entity);
    scene.DestroyEntity(entity);
}

PrefabHandle PrefabInstantiator::GetPrefabForInstance(Entity entity) {
    auto it = s_instances.find(entity);
    return it != s_instances.end() ? it->second : NullPrefab;
}

Entity PrefabInstantiator::InstantiateNode(Scene& scene, const PrefabNode& node, Entity parent,
                                           const math::Transform* overrideRoot) {
    Entity entity = scene.CreateEntity(node.name.empty() ? "Entity" : node.name);
    if (parent != NullEntity) scene.SetParent(entity, parent);

    math::Transform t = node.transform;
    if (overrideRoot) t = *overrideRoot;
    scene.SetTransform(entity, t);

    auto& registry = Prefab::GetRegistry();
    auto& world = scene.World();
    auto& storage = world.Storage().Registry();
    for (const auto& comp : node.components) {
        const auto* ops = registry.GetOps(comp.type);
        if (!ops || !ops->add || !comp.data) continue;
        ops->add(storage, static_cast<entt::entity>(static_cast<entt::id_type>(entity)), comp.data.get());
    }

    for (const auto& child : node.children) {
        InstantiateNode(scene, child, entity, nullptr);
    }
    return entity;
}

} // namespace scene
} // namespace eyelib
