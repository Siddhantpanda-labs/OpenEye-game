#pragma once

#include "eyelib/world/WorldState.h"
#include "eyelib/world/EntityId.h"

#include <utility>
#include <vector>

namespace eyelib {
namespace world {
namespace systems {

/** Sweep-and-prune on X axis. Writes (entityA, entityB) overlap pairs to \a outPairs. Clears \a outPairs first. No narrow phase. */
class BroadPhaseSystem {
public:
    void Update(WorldState& state, std::vector<std::pair<EntityId, EntityId>>& outPairs) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
