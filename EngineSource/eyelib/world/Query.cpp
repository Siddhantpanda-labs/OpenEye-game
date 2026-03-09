#include "Query.h"

namespace eyelib {
namespace world {

QueryResult Query::Execute(const WorldState& state) const {
    QueryResult::Container entityIds;
    entt::registry& registry = const_cast<entt::registry&>(state.GetRegistry());

    if (m_with.empty()) {
        return QueryResult(&state, std::move(entityIds));
    }

    entt::runtime_view view{};
    for (entt::id_type id : m_with) {
        if (auto* storage = registry.storage(id)) {
            view.iterate(*storage);
        } else {
            return QueryResult(&state, std::move(entityIds));
        }
    }

    for (entt::id_type id : m_without) {
        if (auto* storage = registry.storage(id)) {
            view.exclude(*storage);
        }
    }

    for (entt::entity e : view) {
        EntityId id = state.FromEnTT(e);
        if (id != InvalidEntity) {
            entityIds.push_back(id);
        }
    }

    return QueryResult(&state, std::move(entityIds));
}

} // namespace world
} // namespace eyelib
