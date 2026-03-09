#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/world/WorldState.h"
#include "eyelib/world/components/Region.h"

#include <optional>
#include <vector>

namespace eyelib {
namespace world {

bool IsPointInsideRegion(const components::Region& region, const math::Vec3& point);

std::vector<EntityId> GetRegionsContainingPoint(const WorldState& state, const math::Vec3& point);

std::optional<EntityId> GetHighestPriorityRegion(const WorldState& state, const math::Vec3& point);

} // namespace world
} // namespace eyelib
