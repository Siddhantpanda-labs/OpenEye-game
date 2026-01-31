#include "RegionQueries.h"

#include <algorithm>
#include <limits>

namespace eyelib {
namespace world {

bool IsPointInsideRegion(const components::Region& region, const math::Vec3& point) {
    if (!region.enabled) {
        return false;
    }

    if (region.type == components::Region::Type::Sphere) {
        const auto d = point - region.center;
        const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
        return distSq <= (region.radius * region.radius);
    }

    const auto half = region.size * 0.5f;
    const auto min = region.center - half;
    const auto max = region.center + half;
    return (point.x >= min.x && point.x <= max.x) &&
           (point.y >= min.y && point.y <= max.y) &&
           (point.z >= min.z && point.z <= max.z);
}

std::vector<EntityId> GetRegionsContainingPoint(const WorldState& state, const math::Vec3& point) {
    std::vector<EntityId> result;
    const auto& store = state.Components<components::Region>();
    for (const auto& [entity, region] : store.All()) {
        if (IsPointInsideRegion(region, point)) {
            result.push_back(entity);
        }
    }
    return result;
}

std::optional<EntityId> GetHighestPriorityRegion(const WorldState& state, const math::Vec3& point) {
    const auto& store = state.Components<components::Region>();
    std::optional<EntityId> best;
    int bestPriority = std::numeric_limits<int>::min();

    for (const auto& [entity, region] : store.All()) {
        if (!IsPointInsideRegion(region, point)) {
            continue;
        }
        if (!best.has_value() || region.priority > bestPriority) {
            best = entity;
            bestPriority = region.priority;
        }
    }
    return best;
}

} // namespace world
} // namespace eyelib
