#include "Query.h"

#include "entt/entt.hpp"

namespace eyelib {
namespace ecs {

QueryResult Query::Execute(World& world) const {
    entt::registry& registry = world.Registry();
    QueryResult::Container entities;

    if (m_with.empty()) {
        return QueryResult(&registry, {});
    }

    entt::runtime_view view{};
    for (ComponentTypeId id : m_with) {
        if (auto* storage = registry.storage(id)) {
            view.iterate(*storage);
        } else {
            return QueryResult(&registry, {});
        }
    }

    for (ComponentTypeId id : m_without) {
        if (auto* storage = registry.storage(id)) {
            view.exclude(*storage);
        }
    }

    for (auto entity : view) {
        entities.push_back(static_cast<Entity>(static_cast<entt::id_type>(entity)));
    }

    return QueryResult(&registry, std::move(entities));
}

} // namespace ecs
} // namespace eyelib
