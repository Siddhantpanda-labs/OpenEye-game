#pragma once

#include "eyelib/world/WorldState.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/Events/PhysicsEvents.h"

#include <utility>
#include <vector>

namespace eyelib {
namespace world {
namespace systems {

/** Input: overlap pairs from BroadPhaseSystem. Output: ContactData (entityA, entityB, normal, depth, contactPoint) where depth > 0. Uses shape-specific tests; fallback AABB. */
class NarrowPhaseSystem {
public:
    void Update(WorldState& state,
                const std::vector<std::pair<EntityId, EntityId>>& pairs,
                std::vector<ContactEvent>& outContacts) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
