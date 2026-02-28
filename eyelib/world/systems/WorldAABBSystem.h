#pragma once

#include "eyelib/world/WorldState.h"

namespace eyelib {
namespace world {
namespace systems {

/** Simulation-phase: derives world AABB from Transform + local AABB or Collider. Run after TransformSystemPostPhysics. */
class WorldAABBSystem {
public:
    void Update(WorldState& state) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
