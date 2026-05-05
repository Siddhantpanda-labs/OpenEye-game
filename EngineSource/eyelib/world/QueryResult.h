#pragma once

#include "EntityId.h"
#include "WorldState.h"

#include <entt/entt.hpp>

#include <vector>

namespace eyelib {
namespace world {

/** Result of a Query::Execute(). Iterate over EntityIds and read components via Get<T>(entity). Read-only. */
class QueryResult {
public:
    using Container = std::vector<EntityId>;

    QueryResult(const WorldState* state, Container entities)
        : m_state(state), m_entities(std::move(entities)) {}

    size_t GetCount() const { return m_entities.size(); }

    template<typename T>
    const T& Get(EntityId entity) const {
        return m_state->GetRegistry().get<T>(m_state->ToEnTT(entity));
    }

    template<typename T>
    T* TryGet(EntityId entity) const {
        entt::entity e = m_state->ToEnTT(entity);
        if (e == entt::null) { return nullptr; }
        return m_state->GetRegistry().try_get<T>(e);
    }

    class Iterator {
    public:
        using value_type = EntityId;
        Iterator(Container::const_iterator it) : m_it(it) {}
        Iterator& operator++() { ++m_it; return *this; }
        bool operator!=(const Iterator& other) const { return m_it != other.m_it; }
        EntityId operator*() const { return *m_it; }

    private:
        Container::const_iterator m_it;
    };

    Iterator begin() const { return Iterator{m_entities.begin()}; }
    Iterator end() const { return Iterator{m_entities.end()}; }

    const Container& Entities() const { return m_entities; }

private:
    const WorldState* m_state = nullptr;
    Container m_entities;
};

} // namespace world
} // namespace eyelib
