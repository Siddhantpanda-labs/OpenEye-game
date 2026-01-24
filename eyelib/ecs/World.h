#pragma once

#include "Entity.h"
#include "eyelib/math/Transform.h"

#include "entt/entt.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace eyelib {
namespace physics { class PhysicsWorld; }
namespace render { class RenderPipeline; }
namespace ecs {

/**
 * @brief Minimal ECS world (EnTT-backed, hidden in .cpp).
 *
 * For bring-up we expose only Name + Transform. We’ll expand to generic components,
 * queries, and systems once the boundary is stable.
 */
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity CreateEntity();
    Entity CreateEntity(const std::string& name);
    void DestroyEntity(Entity e);
    bool IsAlive(Entity e) const;

    void SetName(Entity e, const std::string& name);
    std::optional<std::string> GetName(Entity e) const;

    void SetTransform(Entity e, const math::Transform& t);
    std::optional<math::Transform> GetTransform(Entity e) const;

    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args);
    template<typename T>
    void RemoveComponent(Entity e);
    template<typename T>
    bool HasComponent(Entity e) const;
    template<typename T>
    T& GetComponent(Entity e);
    template<typename T>
    const T& GetComponent(Entity e) const;
    template<typename T>
    T* TryGetComponent(Entity e);
    template<typename T>
    const T* TryGetComponent(Entity e) const;

    class ComponentRegistry& Components();
    const class ComponentRegistry& Components() const;
    class ComponentStorage& Storage();
    const class ComponentStorage& Storage() const;
    class entt::registry& Registry();
    const class entt::registry& Registry() const;

    void SetPhysicsWorld(::eyelib::physics::PhysicsWorld* world);
    ::eyelib::physics::PhysicsWorld* GetPhysicsWorld() const;
    void SetRenderPipeline(::eyelib::render::RenderPipeline* pipeline);
    ::eyelib::render::RenderPipeline* GetRenderPipeline() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace ecs
} // namespace eyelib

#include "ComponentRegistry.h"
#include "ComponentStorage.h"

namespace eyelib {
namespace ecs {

template<typename T, typename... Args>
T& World::AddComponent(Entity e, Args&&... args) {
    return Storage().AddComponent<T>(e, std::forward<Args>(args)...);
}

template<typename T>
void World::RemoveComponent(Entity e) {
    Storage().RemoveComponent(e, Components().GetComponentType<T>());
}

template<typename T>
bool World::HasComponent(Entity e) const {
    return Storage().HasComponent(e, Components().GetComponentType<T>());
}

template<typename T>
T& World::GetComponent(Entity e) {
    return Storage().Registry().get<T>(detail::ToEnTT(e));
}

template<typename T>
const T& World::GetComponent(Entity e) const {
    return Storage().Registry().get<T>(detail::ToEnTT(e));
}

template<typename T>
T* World::TryGetComponent(Entity e) {
    return Storage().Registry().try_get<T>(detail::ToEnTT(e));
}

template<typename T>
const T* World::TryGetComponent(Entity e) const {
    return Storage().Registry().try_get<T>(detail::ToEnTT(e));
}

} // namespace ecs
} // namespace eyelib
