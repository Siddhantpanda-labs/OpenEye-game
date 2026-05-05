#pragma once

#include "EntityId.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace world {

class WorldState;

/** Stable component type ID (matches EnTT type_hash). Use for reflection, serialization, scripting, inspector. */
using ComponentTypeId = entt::id_type;

/** Type-erased component operations on WorldState. All mutation still flows through commands; these support reflection and by-name access. */
struct ComponentTypeInfo {
    ComponentTypeId id = 0;
    std::string name;
    size_t size = 0;
    size_t alignment = 0;

    std::function<void(WorldState&, EntityId)> add;
    std::function<void(WorldState&, EntityId)> remove;
    std::function<void*(WorldState&, EntityId)> get;
    std::function<bool(WorldState const&, EntityId)> has;
    std::function<size_t(WorldState const&)> count;

    /** Clone component data (for Prefab/copy). Returns allocated copy; caller must free with destroy. */
    std::function<void*(const void*)> clone;
    /** Free component data allocated by clone. */
    std::function<void(void*)> destroy;
};

/** Central registry for component types: stable IDs, reflection (name/size/alignment), and type-erased ops. */
class ComponentRegistry {
public:
    static ComponentRegistry& Get();

    /** Register component type T with a display name. Uses entt::type_hash<T> for stable ID. */
    template<typename T>
    ComponentTypeId Register(const std::string& name);

    template<typename T>
    ComponentTypeId GetComponentType() const;

    size_t GetComponentSize(ComponentTypeId id) const;
    size_t GetComponentAlignment(ComponentTypeId id) const;
    const std::string& GetComponentName(ComponentTypeId id) const;
    ComponentTypeId FindComponentType(const std::string& name) const;
    const ComponentTypeInfo* GetTypeInfo(ComponentTypeId id) const;
    const std::vector<ComponentTypeId>& GetComponentTypes() const { return m_types; }

private:
    ComponentRegistry();
    void RegisterBuiltins();

    std::unordered_map<ComponentTypeId, ComponentTypeInfo> m_info;
    std::vector<ComponentTypeId> m_types;
    std::unordered_map<std::string, ComponentTypeId> m_nameToType;
};

} // namespace world
} // namespace eyelib

#include "ComponentRegistry.inl"
