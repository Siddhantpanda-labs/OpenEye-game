#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>

namespace eyelib {
namespace world {
namespace components {

struct RigidBody {
    uint32_t bodyHandle = 0;
    uint32_t shapeHandle = 0;
    float mass = 1.0f;
    /** Bounciness (0 = inelastic, 1 = fully elastic). Combined with other body for collision response. */
    float restitution = 0.5f;
    /** Friction coefficient (0 = frictionless). Reserved for future use in collision response. */
    float friction = 0.3f;
    bool useGravity = true;
    bool isKinematic = false;
    bool syncTransform = true;
    /** Linear velocity (m/s). Used by Simulation-phase PhysicsSystem for motion integration. */
    math::Vec3 linearVelocity{0.0f, 0.0f, 0.0f};
    /** Cleared after each substep. AddForce(Force) adds here; PhysicsSystem applies (accumulatedForce/mass)*dt then clears. */
    math::Vec3 accumulatedForce{0.0f, 0.0f, 0.0f};
    /** Cleared after each substep. AddTorque adds here; PhysicsSystem applies (accumulatedTorque/mass)*dt then clears. */
    math::Vec3 accumulatedTorque{0.0f, 0.0f, 0.0f};
    /** Angular velocity (rad/s). Integrated into rotation each substep. */
    math::Vec3 angularVelocity{0.0f, 0.0f, 0.0f};
};

} // namespace components
} // namespace world
} // namespace eyelib
