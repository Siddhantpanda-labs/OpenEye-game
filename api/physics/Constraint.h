#pragma once

#include "api/physics/PhysicsWorld.h"
#include "eyelib/physics/constraints/ConstraintFactory.h"

namespace eyelib {
namespace api {

using ConstraintHandle = eyelib::physics::ConstraintHandle;

class Constraint {
public:
    static ConstraintHandle CreateHinge(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
        const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB);

    static ConstraintHandle CreateBallSocket(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB);

    static ConstraintHandle CreateSlider(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);

    static ConstraintHandle CreateFixed(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);

    static ConstraintHandle CreateSpring(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
        float stiffness, float damping);

    static ConstraintHandle Create6DOF(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);

    static ConstraintHandle CreateGear(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB,
        float ratio = 1.0f);

    static ConstraintHandle CreateRope(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
        float length);

    static void Destroy(PhysicsWorld& world, ConstraintHandle handle);
    static void SetHingeLimits(PhysicsWorld& world, ConstraintHandle handle, float lower, float upper);
    static void SetHingeMotor(PhysicsWorld& world, ConstraintHandle handle,
                              bool enabled, float targetVelocity, float maxForce);
};

} // namespace api
} // namespace eyelib
