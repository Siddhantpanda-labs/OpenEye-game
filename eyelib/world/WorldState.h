#pragma once

#include "EntityId.h"

#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace world {

class EntityRegistry;
class IComponentStore;
class Hierarchy;
class Metadata;
class Tags;
template<typename T>
class ComponentStore;

class WorldState {
public:
    WorldState();
    ~WorldState();

    WorldState(const WorldState&) = delete;
    WorldState& operator=(const WorldState&) = delete;
    WorldState(WorldState&&) noexcept = default;
    WorldState& operator=(WorldState&&) noexcept = default;

    EntityRegistry& Entities();
    const EntityRegistry& Entities() const;

    Hierarchy& GraphHierarchy();
    const Hierarchy& GraphHierarchy() const;

    Metadata& GraphMetadata();
    const Metadata& GraphMetadata() const;

    Tags& GraphTags();
    const Tags& GraphTags() const;

    template<typename T>
    ComponentStore<T>& Components();

    template<typename T>
    const ComponentStore<T>& Components() const;

    const std::unordered_map<size_t, std::unique_ptr<IComponentStore>>& ComponentStores() const;
    IComponentStore* FindComponentStore(size_t typeKey);
    const IComponentStore* FindComponentStore(size_t typeKey) const;
    void RegisterComponentStore(size_t typeKey, std::unique_ptr<IComponentStore> store);

    void Clear();

private:
    std::unique_ptr<EntityRegistry> m_entities;
    std::unique_ptr<Hierarchy> m_hierarchy;
    std::unique_ptr<Metadata> m_metadata;
    std::unique_ptr<Tags> m_tags;

    std::unordered_map<size_t, std::unique_ptr<IComponentStore>> m_componentStores;
};

} // namespace world
} // namespace eyelib

#include "ComponentStorage/ComponentStore.h"
#include "ComponentStorage/IComponentStore.h"

namespace eyelib {
namespace world {

template<typename T>
ComponentStore<T>& WorldState::Components() {
    const size_t typeKey = typeid(T).hash_code();
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        auto store = std::make_unique<ComponentStore<T>>();
        auto* raw = store.get();
        m_componentStores.emplace(typeKey, std::move(store));
        return *raw;
    }
    return *static_cast<ComponentStore<T>*>(it->second.get());
}

template<typename T>
const ComponentStore<T>& WorldState::Components() const {
    const size_t typeKey = typeid(T).hash_code();
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        static ComponentStore<T> s_empty;
        return s_empty;
    }
    return *static_cast<const ComponentStore<T>*>(it->second.get());
}

} // namespace world
} // namespace eyelib
