#pragma once

#include "entt/entt.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace ecs {

using ComponentTypeId = entt::id_type;

struct ComponentOps {
    using AddFn = void(*)(entt::registry&, entt::entity, const void*);
    using RemoveFn = void(*)(entt::registry&, entt::entity);
    using GetFn = void*(*)(entt::registry&, entt::entity);
    using GetConstFn = const void*(*)(const entt::registry&, entt::entity);
    using HasFn = bool(*)(const entt::registry&, entt::entity);
    using CountFn = size_t(*)(const entt::registry&);
    using CloneFn = void*(*)(const void*);
    using DestroyFn = void(*)(void*);

    AddFn add = nullptr;
    RemoveFn remove = nullptr;
    GetFn get = nullptr;
    GetConstFn getConst = nullptr;
    HasFn has = nullptr;
    CountFn count = nullptr;
    CloneFn clone = nullptr;
    DestroyFn destroy = nullptr;
};

class ComponentRegistry {
public:
    ComponentRegistry() = default;

    template<typename T>
    ComponentTypeId RegisterComponent();

    template<typename T>
    ComponentTypeId GetComponentType() const;

    size_t GetComponentSize(ComponentTypeId id) const;
    size_t GetComponentAlignment(ComponentTypeId id) const;
    const std::string& GetComponentName(ComponentTypeId id) const;
    ComponentTypeId FindComponentType(const std::string& name) const;
    const ComponentOps* GetOps(ComponentTypeId id) const;
    const std::vector<ComponentTypeId>& GetComponentTypes() const { return m_types; }

private:
    struct ComponentInfo {
        size_t size = 0;
        size_t alignment = 0;
        std::string name;
        ComponentOps ops{};
    };

    std::unordered_map<ComponentTypeId, ComponentInfo> m_info;
    std::vector<ComponentTypeId> m_types;
    std::unordered_map<std::string, ComponentTypeId> m_nameToType;
};

} // namespace ecs
} // namespace eyelib

// Template implementation
namespace eyelib {
namespace ecs {

namespace detail {
template<typename T>
void AddComponent(entt::registry& registry, entt::entity e, const void* data) {
    const T* typed = static_cast<const T*>(data);
    if (typed) {
        registry.emplace_or_replace<T>(e, *typed);
    } else {
        registry.emplace_or_replace<T>(e);
    }
}

template<typename T>
void RemoveComponent(entt::registry& registry, entt::entity e) {
    if (registry.any_of<T>(e)) {
        registry.remove<T>(e);
    }
}

template<typename T>
void* GetComponent(entt::registry& registry, entt::entity e) {
    return registry.try_get<T>(e);
}

template<typename T>
const void* GetComponentConst(const entt::registry& registry, entt::entity e) {
    return registry.try_get<T>(e);
}

template<typename T>
bool HasComponent(const entt::registry& registry, entt::entity e) {
    return registry.any_of<T>(e);
}

template<typename T>
size_t GetComponentCount(const entt::registry& registry) {
    if (const auto* storage = registry.storage<T>()) {
        return storage->size();
    }
    return 0;
}

template<typename T>
void* CloneComponent(const void* data) {
    if (!data) return new T();
    return new T(*static_cast<const T*>(data));
}

template<typename T>
void DestroyComponent(void* data) {
    delete static_cast<T*>(data);
}
} // namespace detail

template<typename T>
ComponentTypeId ComponentRegistry::RegisterComponent() {
    const ComponentTypeId id = entt::type_hash<T>::value();
    if (m_info.find(id) != m_info.end()) {
        return id;
    }
    ComponentInfo info;
    info.size = sizeof(T);
    info.alignment = alignof(T);
    info.name = std::string(entt::type_name<T>::value());
    info.ops.add = &detail::AddComponent<T>;
    info.ops.remove = &detail::RemoveComponent<T>;
    info.ops.get = &detail::GetComponent<T>;
    info.ops.getConst = &detail::GetComponentConst<T>;
    info.ops.has = &detail::HasComponent<T>;
    info.ops.count = &detail::GetComponentCount<T>;
    info.ops.clone = &detail::CloneComponent<T>;
    info.ops.destroy = &detail::DestroyComponent<T>;
    const std::string name = info.name;
    m_info.emplace(id, std::move(info));
    m_types.push_back(id);
    m_nameToType[name] = id;
    return id;
}

template<typename T>
ComponentTypeId ComponentRegistry::GetComponentType() const {
    return entt::type_hash<T>::value();
}

} // namespace ecs
} // namespace eyelib
