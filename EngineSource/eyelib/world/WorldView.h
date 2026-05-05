#pragma once

#include "WorldState.h"
#include "ComponentStorage/TransformStore.h"
#include "eyelib/world/components/Transform.h"
#include "Query.h"

#include <type_traits>
#include <utility>

namespace eyelib {
namespace world {

class WorldView {
public:
    explicit WorldView(const WorldState& state)
        : m_state(&state) {}

    const WorldState& State() const { return *m_state; }

    const EntityRegistry& Entities() const { return m_state->Entities(); }
    const Hierarchy& GraphHierarchy() const { return m_state->GraphHierarchy(); }
    const Metadata& GraphMetadata() const { return m_state->GraphMetadata(); }
    const Tags& GraphTags() const { return m_state->GraphTags(); }

    template<typename T>
    const ComponentStore<T>& Components() const {
        return m_state->Components<T>();
    }

    /** Typed component read (from EnTT registry). Returns nullptr if entity has no mapping or no component. Mutation must go through commands. */
    template<typename T>
    T* TryGetComponent(EntityId entity) {
        entt::entity e = m_state->ToEnTT(entity);
        if (e == entt::null) { return nullptr; }
        return const_cast<T*>(std::as_const(m_state->GetRegistry()).try_get<T>(e));
    }
    template<typename T>
    const T* TryGetComponent(EntityId entity) const {
        entt::entity e = m_state->ToEnTT(entity);
        if (e == entt::null) { return nullptr; }
        return m_state->GetRegistry().try_get<T>(e);
    }
    /** Typed component read. Undefined behavior if entity or component missing. Prefer TryGetComponent for safe access. */
    template<typename T>
    T& GetComponent(EntityId entity) {
        return *m_state->GetRegistry().get<T>(m_state->ToEnTT(entity));
    }
    template<typename T>
    const T& GetComponent(EntityId entity) const {
        return *m_state->GetRegistry().get<T>(m_state->ToEnTT(entity));
    }
    template<typename T>
    bool HasComponent(EntityId entity) const {
        entt::entity e = m_state->ToEnTT(entity);
        if (e == entt::null) { return false; }
        return m_state->GetRegistry().any_of<T>(e);
    }

    /** SoA transform store. Use instead of Components<components::Transform> for cache-friendly access. */
    const TransformStore& GetTransformStore() const { return m_state->GetTransformStore(); }

    const std::unordered_map<size_t, std::unique_ptr<IComponentStore>>& ComponentStores() const {
        return m_state->ComponentStores();
    }

    /** Build a query over the registry. Example: view.CreateQuery().With<Transform>().With<Renderable>().Execute() */
    Query CreateQuery() const { return Query(m_state); }

private:
    const WorldState* m_state = nullptr;
};

} // namespace world
} // namespace eyelib
