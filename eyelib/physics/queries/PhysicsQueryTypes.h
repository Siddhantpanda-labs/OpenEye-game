#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"
#include <vector>

namespace eyelib {
namespace physics {

struct RaycastResult {
    bool hit = false;
    math::Vec3 point{0.0f};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    RigidBodyHandle body = 0;
};

struct SweepResult {
    bool hit = false;
    math::Vec3 point{0.0f};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    RigidBodyHandle body = 0;
    float fraction = 0.0f;
};

struct OverlapResult {
    RigidBodyHandle body = 0;
    math::Vec3 contactPoint{0.0f};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
};

} // namespace physics
} // namespace eyelib
