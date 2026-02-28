#include "BroadPhaseSystem.h"

#include "eyelib/world/components/WorldAABB.h"
#include <algorithm>

namespace eyelib {
namespace world {
namespace systems {

static bool AABBOverlaps(const components::WorldAABB& a, const components::WorldAABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

void BroadPhaseSystem::Update(WorldState& state, std::vector<std::pair<EntityId, EntityId>>& outPairs) const {
    outPairs.clear();
    const auto& worldAabbs = state.Components<components::WorldAABB>();
    std::vector<std::pair<EntityId, components::WorldAABB>> list;
    list.reserve(worldAabbs.All().size());
    for (const auto& [entity, aabb] : worldAabbs.All())
        list.push_back({entity, aabb});

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.second.min.x < b.second.min.x;
    });

    for (size_t i = 0; i < list.size(); ++i) {
        const float maxX = list[i].second.max.x;
        for (size_t j = i + 1; j < list.size(); ++j) {
            if (list[j].second.min.x > maxX)
                break;
            if (AABBOverlaps(list[i].second, list[j].second))
                outPairs.push_back({list[i].first, list[j].first});
        }
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
