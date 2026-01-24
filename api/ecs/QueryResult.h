#pragma once

#include "Component.h"
#include "Entity.h"

#include "eyelib/ecs/QueryResult.h"

namespace eyelib {
namespace api {

class QueryResult {
public:
    class Item {
    public:
        Item(eyelib::ecs::QueryResult::Item internal) : m_internal(internal) {}

        Entity GetEntity() const { return m_internal.GetEntity(); }

        template<typename T>
        T Get() const {
            using Internal = typename detail::ComponentConverter<T>::Internal;
            return detail::ComponentConverter<T>::FromInternal(m_internal.Get<Internal>());
        }

        template<typename T>
        std::optional<T> TryGet() const {
            using Internal = typename detail::ComponentConverter<T>::Internal;
            if (auto* comp = m_internal.TryGet<Internal>()) {
                return detail::ComponentConverter<T>::FromInternal(*comp);
            }
            return std::nullopt;
        }

    private:
        eyelib::ecs::QueryResult::Item m_internal;
    };

    QueryResult(eyelib::ecs::QueryResult internal) : m_internal(std::move(internal)) {}

    size_t GetCount() const { return m_internal.GetCount(); }

    class Iterator {
    public:
        Iterator(eyelib::ecs::QueryResult::Iterator it) : m_it(it) {}
        Iterator& operator++() { ++m_it; return *this; }
        bool operator!=(const Iterator& other) const { return m_it != other.m_it; }
        Item operator*() const { return Item{ *m_it }; }
    private:
        eyelib::ecs::QueryResult::Iterator m_it;
    };

    Iterator begin() const { return Iterator{ m_internal.begin() }; }
    Iterator end() const { return Iterator{ m_internal.end() }; }

private:
    eyelib::ecs::QueryResult m_internal;
};

} // namespace api
} // namespace eyelib
