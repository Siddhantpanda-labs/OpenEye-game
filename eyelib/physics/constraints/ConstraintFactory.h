#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace physics {

using ConstraintHandle = PhysicsWorld::ConstraintHandle;

/**
 * @brief Factory for creating and managing physics constraints. Uses body handles,
 * delegates to PhysicsWorld. No Bullet types exposed.
 */
class ConstraintFactory {
public:
    static ConstraintHandle CreateHinge(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB,
        const math::Vec3& axisA, const math::Vec3& axisB);

    static ConstraintHandle CreateBallSocket(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB);

    static ConstraintHandle CreateSlider(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);

    static ConstraintHandle CreateFixed(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);

    static ConstraintHandle CreateSpring(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB,
        float stiffness, float damping);

    static ConstraintHandle Create6DOF(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);

    static ConstraintHandle CreateGear(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& axisA, const math::Vec3& axisB,
        float ratio = 1.0f);

    /** Rope: max-distance constraint between two pivots (implemented via spring). */
    static ConstraintHandle CreateRope(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB,
        float length);

    static void DestroyConstraint(PhysicsWorld* world, ConstraintHandle handle);

    /** Hinge limits (radians). */
    static void SetHingeLimits(PhysicsWorld* world, ConstraintHandle handle, float lower, float upper);

    /** Hinge motor. */
    static void SetHingeMotor(PhysicsWorld* world, ConstraintHandle handle,
                             bool enabled, float targetVelocity, float maxForce);
};

} // namespace physics
} // namespace eyelib
