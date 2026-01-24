#pragma once

#include "ComponentRegistry.h"
#include "Entity.h"

#include "entt/entt.hpp"

#include <cstddef>
#include <utility>

namespace eyelib {
namespace ecs {

class ComponentStorage {
public:
    explicit ComponentStorage(ComponentRegistry& registry);

    entt::registry& Registry() { return m_registry; }
    const entt::registry& Registry() const { return m_registry; }

    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args);

    void* AddComponent(Entity e, ComponentTypeId id, const void* data);
    void RemoveComponent(Entity e, ComponentTypeId id);
    void* GetComponent(Entity e, ComponentTypeId id);
    const void* GetComponent(Entity e, ComponentTypeId id) const;
    bool HasComponent(Entity e, ComponentTypeId id) const;
    size_t GetComponentCount(ComponentTypeId id) const;

private:
    ComponentRegistry& m_registryInfo;
    entt::registry m_registry{};
};

} // namespace ecs
} // namespace eyelib

namespace eyelib {
namespace ecs {

namespace detail {
inline entt::entity ToEnTT(Entity e) {
    return static_cast<entt::entity>(static_cast<entt::id_type>(e));
}
} // namespace detail

template<typename T, typename... Args>
T& ComponentStorage::AddComponent(Entity e, Args&&... args) {
    return m_registry.emplace_or_replace<T>(detail::ToEnTT(e), std::forward<Args>(args)...);
}

} // namespace ecs
} // namespace eyelib
