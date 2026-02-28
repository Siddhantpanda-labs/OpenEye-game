#pragma once

#include "eyelib/world/WorldState.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/Events/PhysicsEvents.h"

#include <functional>
#include <vector>

namespace eyelib {
namespace world {
namespace systems {

/** Runs after NarrowPhaseSystem. Input: FrameContactBuffer. Applies mass-weighted impulse (combined restitution), enqueues ContactEvent for each contact. Skips kinematic for impulse; still enqueues. */
class CollisionResponseSystem {
public:
    void Update(WorldState& state, const std::vector<ContactEvent>& contacts, std::function<void(const ContactEvent&)> enqueueContact);
};

} // namespace systems
} // namespace world
} // namespace eyelib
