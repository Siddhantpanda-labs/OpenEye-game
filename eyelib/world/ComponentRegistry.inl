#include "ComponentRegistry.h"
#include "WorldState.h"

namespace eyelib {
namespace world {

template<typename T>
ComponentTypeId ComponentRegistry::Register(const std::string& name) {
    const ComponentTypeId id = entt::type_hash<T>::value();
    if (m_info.find(id) != m_info.end()) {
        return id;
    }
    ComponentTypeInfo info;
    info.id = id;
    info.name = name;
    info.size = sizeof(T);
    info.alignment = alignof(T);
    info.add = [](WorldState& state, EntityId entity) {
        entt::entity e = state.ToEnTT(entity);
        if (e != entt::null) {
            state.GetRegistry().emplace_or_replace<T>(e);
        }
    };
    info.remove = [](WorldState& state, EntityId entity) {
        entt::entity e = state.ToEnTT(entity);
        if (e != entt::null) {
            state.GetRegistry().remove<T>(e);
        }
    };
    info.get = [](WorldState& state, EntityId entity) -> void* {
        entt::entity e = state.ToEnTT(entity);
        if (e == entt::null) { return nullptr; }
        return state.GetRegistry().try_get<T>(e);
    };
    info.has = [](WorldState const& state, EntityId entity) -> bool {
        entt::entity e = state.ToEnTT(entity);
        if (e == entt::null) { return false; }
        return state.GetRegistry().any_of<T>(e);
    };
    info.count = [](WorldState const& state) -> size_t {
        if (auto const* storage = state.GetRegistry().storage<T>()) {
            return storage->size();
        }
        return 0;
    };
    info.clone = [](const void* p) -> void* {
        return p ? new T(*static_cast<const T*>(p)) : nullptr;
    };
    info.destroy = [](void* p) {
        delete static_cast<T*>(p);
    };
    m_info[id] = std::move(info);
    m_types.push_back(id);
    m_nameToType[name] = id;
    return id;
}

template<typename T>
ComponentTypeId ComponentRegistry::GetComponentType() const {
    return entt::type_hash<T>::value();
}

} // namespace world
} // namespace eyelib
