#pragma once

#include "QueryResult.h"
#include "WorldState.h"

#include <entt/entt.hpp>

#include <vector>

namespace eyelib {
namespace world {

/** Builder for declarative entity queries over the registry. Use With<T>(), Without<T>(), then Execute() or Execute(state). */
class Query {
public:
    Query() : m_state(nullptr) {}
    /** Bind to a state so Execute() can be called with no arguments. */
    explicit Query(const WorldState* state) : m_state(state) {}

    template<typename T>
    Query& With() {
        m_with.push_back(entt::type_hash<T>::value());
        return *this;
    }

    template<typename T>
    Query& Without() {
        m_without.push_back(entt::type_hash<T>::value());
        return *this;
    }

    /** Execute against the given world state. Returns entities that have all With<> components and none of the Without<> components. */
    QueryResult Execute(const WorldState& state) const;
    /** Execute against the bound state. Use when this Query was created via WorldView::Query() or World::Query(). */
    QueryResult Execute() const { return Execute(*m_state); }

private:
    const WorldState* m_state = nullptr;
    std::vector<entt::id_type> m_with;
    std::vector<entt::id_type> m_without;
};

} // namespace world
} // namespace eyelib
