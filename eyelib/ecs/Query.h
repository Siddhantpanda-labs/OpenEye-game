#pragma once

#include "ComponentRegistry.h"
#include "QueryResult.h"
#include "World.h"

#include <vector>

namespace eyelib {
namespace ecs {

class Query {
public:
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

    template<typename T>
    Query& Has() {
        m_optional.push_back(entt::type_hash<T>::value());
        return *this;
    }

    QueryResult Execute(World& world) const;

private:
    std::vector<ComponentTypeId> m_with;
    std::vector<ComponentTypeId> m_without;
    std::vector<ComponentTypeId> m_optional;
};

} // namespace ecs
} // namespace eyelib
