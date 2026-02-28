#pragma once

#include "api/physics/PhysicsWorld.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace api {
namespace physics {

using ConstraintHandle = uint32_t;

enum class ConstraintType : uint8_t {
    Unknown = 0,
    Hinge,
    BallSocket,
    Slider,
    Fixed,
    Spring,
    SixDof,
    Gear,
    Rope
};

class Constraint {
public:
    static ConstraintHandle CreateHinge(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
        const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB);
    static ConstraintHandle CreateHingeWorld(PhysicsWorld& world,
        RigidBodyHandle body, const eyelib::math::Vec3& pivot, const eyelib::math::Vec3& axis);

    static ConstraintHandle CreateBallSocket(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB);

    static ConstraintHandle CreateSlider(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);
    static ConstraintHandle CreateSliderWorld(PhysicsWorld& world,
        RigidBodyHandle body, const eyelib::math::Mat4& frame);

    static ConstraintHandle CreateFixed(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);
    static ConstraintHandle CreateFixedWorld(PhysicsWorld& world,
        RigidBodyHandle body, const eyelib::math::Mat4& frame);

    static ConstraintHandle CreateSpring(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
        float stiffness, float damping);

    static ConstraintHandle Create6DOF(PhysicsWorld& world,
        RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB);
    static ConstraintHandle Create6DOFWorld(PhysicsWorld& world,
        RigidBodyHandle body, const eyelib::math::Mat4& frame);

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
    static void SetSliderLimits(PhysicsWorld& world, ConstraintHandle handle,
                                float lowerLin, float upperLin, float lowerAng, float upperAng);
    static void SetSliderMotor(PhysicsWorld& world, ConstraintHandle handle,
                               bool linearEnabled, float targetVelocity, float maxForce,
                               bool angularEnabled, float targetAngVelocity, float maxAngForce);
    static void Set6DOFLimits(PhysicsWorld& world, ConstraintHandle handle,
                              const eyelib::math::Vec3& linearLower, const eyelib::math::Vec3& linearUpper,
                              const eyelib::math::Vec3& angularLower, const eyelib::math::Vec3& angularUpper);
    static void Set6DOFSpring(PhysicsWorld& world, ConstraintHandle handle,
                              int axis, bool enabled, float stiffness, float damping);
    static void Set6DOFEquilibrium(PhysicsWorld& world, ConstraintHandle handle, int axis = -1);
    static void SetBreakingImpulseThreshold(PhysicsWorld& world, ConstraintHandle handle, float threshold);
    static ConstraintType GetConstraintType(PhysicsWorld& world, ConstraintHandle handle);
    static bool GetConstraintBodies(PhysicsWorld& world, ConstraintHandle handle,
                                    RigidBodyHandle& bodyA, RigidBodyHandle& bodyB);
};

} // namespace physics
} // namespace api
} // namespace eyelib
