#include "eyelib/physics/constraints/ConstraintFactory.h"

namespace eyelib {
namespace physics {

ConstraintHandle ConstraintFactory::CreateHinge(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB,
    const math::Vec3& axisA, const math::Vec3& axisB) {
    return world ? world->CreateHingeConstraint(bodyA, bodyB, pivotA, pivotB, axisA, axisB) : 0;
}

ConstraintHandle ConstraintFactory::CreateBallSocket(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB) {
    return world ? world->CreateBallSocketConstraint(bodyA, bodyB, pivotA, pivotB) : 0;
}

ConstraintHandle ConstraintFactory::CreateSlider(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    return world ? world->CreateSliderConstraint(bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle ConstraintFactory::CreateFixed(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    return world ? world->CreateFixedConstraint(bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle ConstraintFactory::CreateSpring(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB,
    float stiffness, float damping) {
    return world ? world->CreateSpringConstraint(bodyA, bodyB, pivotA, pivotB, stiffness, damping) : 0;
}

ConstraintHandle ConstraintFactory::Create6DOF(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    return world ? world->Create6DOFConstraint(bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle ConstraintFactory::CreateGear(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& axisA, const math::Vec3& axisB,
    float ratio) {
    return world ? world->CreateGearConstraint(bodyA, bodyB, axisA, axisB, ratio) : 0;
}

ConstraintHandle ConstraintFactory::CreateRope(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB,
    float length) {
    (void)length;
    return world ? world->CreateRopeConstraint(bodyA, bodyB, pivotA, pivotB, length) : 0;
}

void ConstraintFactory::DestroyConstraint(PhysicsWorld* world, ConstraintHandle handle) {
    if (world) world->DestroyConstraint(handle);
}

void ConstraintFactory::SetHingeLimits(PhysicsWorld* world, ConstraintHandle handle, float lower, float upper) {
    if (world) world->SetHingeLimits(handle, lower, upper);
}

void ConstraintFactory::SetHingeMotor(PhysicsWorld* world, ConstraintHandle handle,
                                     bool enabled, float targetVelocity, float maxForce) {
    if (world) world->SetHingeMotor(handle, enabled, targetVelocity, maxForce);
}

} // namespace physics
} // namespace eyelib
