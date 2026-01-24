#pragma once

#include "eyelib/physics/softbody/SoftBodyFactory.h"

namespace eyelib {
namespace physics {

/**
 * @brief Cloth utilities for soft-body tuning.
 */
class Cloth {
public:
    static void SetBending(PhysicsWorld* world, SoftBodyHandle cloth, int distance, float stiffness);
    static void SetWind(PhysicsWorld* world, SoftBodyHandle cloth, const math::Vec3& direction, float strength);
    static void SetCollision(PhysicsWorld* world, SoftBodyHandle cloth, bool enabled);
    static void SetSelfCollision(PhysicsWorld* world, SoftBodyHandle cloth, bool enabled);
    static void SetDamping(PhysicsWorld* world, SoftBodyHandle cloth, float damping);
};

} // namespace physics
} // namespace eyelib
