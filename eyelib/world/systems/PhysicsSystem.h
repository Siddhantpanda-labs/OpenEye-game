#pragma once

#include "eyelib/world/WorldState.h"
#include "eyelib/world/Events/PhysicsEvents.h"
#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace world {
namespace systems {

/**
 * Simulation-phase system: applies queued forces/impulses, then integrates RigidBody
 * linearVelocity (accumulatedForce/mass*dt, gravity), writes local position, marks dirty.
 * TransformSystem must run after to recompute world matrices.
 */
class PhysicsSystem {
public:
    void Update(WorldState& state, float dt, const math::Vec3& gravity,
                std::vector<PendingForceEntry>& pendingForces,
                std::vector<PendingImpulseEntry>& pendingImpulses,
                std::vector<PendingTorqueEntry>& pendingTorques,
                std::vector<PendingAngularImpulseEntry>& pendingAngularImpulses) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
