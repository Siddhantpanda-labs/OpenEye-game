#pragma once

#include "Component.h"
#include "QueryResult.h"
#include "World.h"

#include "eyelib/ecs/Query.h"

namespace eyelib {
namespace api {

class Query {
public:
    template<typename T>
    Query& With() {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        m_query.With<Internal>();
        return *this;
    }

    template<typename T>
    Query& Without() {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        m_query.Without<Internal>();
        return *this;
    }

    template<typename T>
    Query& Has() {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        m_query.Has<Internal>();
        return *this;
    }

    QueryResult Execute(World& world) {
        return QueryResult(m_query.Execute(*world.GetInternal()));
    }

private:
    eyelib::ecs::Query m_query;
};

} // namespace api
} // namespace eyelib
