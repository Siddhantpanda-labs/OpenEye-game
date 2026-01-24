#pragma once

#include "ComponentStorage.h"
#include "Entity.h"

#include "entt/entt.hpp"

#include <vector>

namespace eyelib {
namespace ecs {

class QueryResult {
public:
    struct Item {
        entt::registry* registry = nullptr;
        Entity entity = NullEntity;

        Entity GetEntity() const { return entity; }

        template<typename T>
        T& Get() const {
            return registry->get<T>(detail::ToEnTT(entity));
        }

        template<typename T>
        T* TryGet() const {
            return registry->try_get<T>(detail::ToEnTT(entity));
        }
    };

    using Container = std::vector<Entity>;

    QueryResult(entt::registry* reg, Container entities)
        : m_registry(reg), m_entities(std::move(entities)) {}

    size_t GetCount() const { return m_entities.size(); }

    class Iterator {
    public:
        Iterator(entt::registry* reg, Container::const_iterator it)
            : m_registry(reg), m_it(it) {}

        Iterator& operator++() { ++m_it; return *this; }
        bool operator!=(const Iterator& other) const { return m_it != other.m_it; }

        Item operator*() const { return Item{ m_registry, *m_it }; }

    private:
        entt::registry* m_registry;
        Container::const_iterator m_it;
    };

    Iterator begin() const { return Iterator{ m_registry, m_entities.begin() }; }
    Iterator end() const { return Iterator{ m_registry, m_entities.end() }; }

private:
    entt::registry* m_registry = nullptr;
    Container m_entities;
};

} // namespace ecs
} // namespace eyelib
