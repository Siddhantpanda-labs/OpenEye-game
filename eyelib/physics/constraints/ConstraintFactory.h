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
    static ConstraintHandle CreateHingeWorld(
        PhysicsWorld* world,
        RigidBodyHandle body,
        const math::Vec3& pivot, const math::Vec3& axis);

    static ConstraintHandle CreateBallSocket(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB);

    static ConstraintHandle CreateSlider(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    static ConstraintHandle CreateSliderWorld(
        PhysicsWorld* world,
        RigidBodyHandle body, const math::Mat4& frame);

    static ConstraintHandle CreateFixed(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    static ConstraintHandle CreateFixedWorld(
        PhysicsWorld* world,
        RigidBodyHandle body, const math::Mat4& frame);

    static ConstraintHandle CreateSpring(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB,
        float stiffness, float damping);

    static ConstraintHandle Create6DOF(
        PhysicsWorld* world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    static ConstraintHandle Create6DOFWorld(
        PhysicsWorld* world,
        RigidBodyHandle body, const math::Mat4& frame);

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
    static void SetSliderLimits(PhysicsWorld* world, ConstraintHandle handle,
                                float lowerLin, float upperLin, float lowerAng, float upperAng);
    static void SetSliderMotor(PhysicsWorld* world, ConstraintHandle handle,
                               bool linearEnabled, float targetVelocity, float maxForce,
                               bool angularEnabled, float targetAngVelocity, float maxAngForce);
    static void Set6DOFLimits(PhysicsWorld* world, ConstraintHandle handle,
                              const math::Vec3& linearLower, const math::Vec3& linearUpper,
                              const math::Vec3& angularLower, const math::Vec3& angularUpper);
    static void Set6DOFSpring(PhysicsWorld* world, ConstraintHandle handle,
                              int axis, bool enabled, float stiffness, float damping);
    static void Set6DOFEquilibrium(PhysicsWorld* world, ConstraintHandle handle, int axis = -1);
    static void SetBreakingImpulseThreshold(PhysicsWorld* world, ConstraintHandle handle, float threshold);
    static PhysicsWorld::ConstraintType GetConstraintType(PhysicsWorld* world, ConstraintHandle handle);
    static bool GetConstraintBodies(PhysicsWorld* world, ConstraintHandle handle,
                                    RigidBodyHandle& bodyA, RigidBodyHandle& bodyB);
};

} // namespace physics
} // namespace eyelib
