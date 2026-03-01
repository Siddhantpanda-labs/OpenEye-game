#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>

namespace eyelib {
namespace world {
namespace components {

struct RigidBody {
    float mass = 1.0f;
    float restitution = 0.5f;
    float friction = 0.3f;
    bool useGravity = true;
    bool isKinematic = false;
    bool syncTransform = true;
    math::Vec3 linearVelocity{0.0f, 0.0f, 0.0f};
    math::Vec3 accumulatedForce{0.0f, 0.0f, 0.0f};
    math::Vec3 accumulatedTorque{0.0f, 0.0f, 0.0f};
    math::Vec3 angularVelocity{0.0f, 0.0f, 0.0f};
};

} // namespace components
} // namespace world
} // namespace eyelib
