#include "WorldObservation.h"

#include "eyelib/world/RegionQueries.h"
#include "eyelib/world/Serialization/ComponentSerializer.h"
#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/components/Transform.h"

namespace eyelib {
namespace world {

WorldObservation::WorldObservation(WorldView view)
    : m_view(std::move(view)) {}

std::vector<EntityId> WorldObservation::GetEntities() const {
    return m_view.Entities().GetAliveEntities();
}

std::optional<std::string> WorldObservation::GetEntityName(EntityId entity) const {
    auto entry = m_view.GraphMetadata().Get(entity);
    if (!entry.has_value()) {
        return std::nullopt;
    }
    return entry->name;
}

std::optional<math::Transform> WorldObservation::GetTransform(EntityId entity) const {
    const auto& store = m_view.Components<components::Transform>();
    if (const auto* t = store.TryGet(entity)) {
        return *t;
    }
    return std::nullopt;
}

std::vector<std::string> WorldObservation::GetComponentNames(EntityId entity) const {
    std::vector<std::string> names;
    const auto& stores = m_view.ComponentStores();
    auto& serializer = ComponentSerializer::Get();
    for (const auto& [typeKey, store] : stores) {
        if (!store || !store->Has(entity)) {
            continue;
        }
        const auto* desc = serializer.GetDescriptor(typeKey);
        if (desc && !desc->name.empty()) {
            names.push_back(desc->name);
        }
    }
    return names;
}

std::vector<EntityId> WorldObservation::GetRegionsContainingPoint(const math::Vec3& point) const {
    return world::GetRegionsContainingPoint(m_view.State(), point);
}

std::optional<EntityId> WorldObservation::GetHighestPriorityRegion(const math::Vec3& point) const {
    return world::GetHighestPriorityRegion(m_view.State(), point);
}

} // namespace world
} // namespace eyelib
