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

ConstraintHandle ConstraintFactory::CreateHingeWorld(
    PhysicsWorld* world,
    RigidBodyHandle body,
    const math::Vec3& pivot, const math::Vec3& axis) {
    return world ? world->CreateHingeConstraintWorld(body, pivot, axis) : 0;
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

ConstraintHandle ConstraintFactory::CreateSliderWorld(
    PhysicsWorld* world,
    RigidBodyHandle body, const math::Mat4& frame) {
    return world ? world->CreateSliderConstraintWorld(body, frame) : 0;
}

ConstraintHandle ConstraintFactory::CreateFixed(
    PhysicsWorld* world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    return world ? world->CreateFixedConstraint(bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle ConstraintFactory::CreateFixedWorld(
    PhysicsWorld* world,
    RigidBodyHandle body, const math::Mat4& frame) {
    return world ? world->CreateFixedConstraintWorld(body, frame) : 0;
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

ConstraintHandle ConstraintFactory::Create6DOFWorld(
    PhysicsWorld* world,
    RigidBodyHandle body, const math::Mat4& frame) {
    return world ? world->Create6DOFConstraintWorld(body, frame) : 0;
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

void ConstraintFactory::SetSliderLimits(PhysicsWorld* world, ConstraintHandle handle,
                                        float lowerLin, float upperLin, float lowerAng, float upperAng) {
    if (world) world->SetSliderLimits(handle, lowerLin, upperLin, lowerAng, upperAng);
}

void ConstraintFactory::SetSliderMotor(PhysicsWorld* world, ConstraintHandle handle,
                                       bool linearEnabled, float targetVelocity, float maxForce,
                                       bool angularEnabled, float targetAngVelocity, float maxAngForce) {
    if (world) world->SetSliderMotor(handle, linearEnabled, targetVelocity, maxForce,
                                     angularEnabled, targetAngVelocity, maxAngForce);
}

void ConstraintFactory::Set6DOFLimits(PhysicsWorld* world, ConstraintHandle handle,
                                      const math::Vec3& linearLower, const math::Vec3& linearUpper,
                                      const math::Vec3& angularLower, const math::Vec3& angularUpper) {
    if (world) world->Set6DOFLimits(handle, linearLower, linearUpper, angularLower, angularUpper);
}

void ConstraintFactory::Set6DOFSpring(PhysicsWorld* world, ConstraintHandle handle,
                                      int axis, bool enabled, float stiffness, float damping) {
    if (world) world->Set6DOFSpring(handle, axis, enabled, stiffness, damping);
}

void ConstraintFactory::Set6DOFEquilibrium(PhysicsWorld* world, ConstraintHandle handle, int axis) {
    if (world) world->Set6DOFEquilibrium(handle, axis);
}

void ConstraintFactory::SetBreakingImpulseThreshold(PhysicsWorld* world, ConstraintHandle handle, float threshold) {
    if (world) world->SetConstraintBreakingImpulseThreshold(handle, threshold);
}

PhysicsWorld::ConstraintType ConstraintFactory::GetConstraintType(PhysicsWorld* world, ConstraintHandle handle) {
    return world ? world->GetConstraintType(handle) : PhysicsWorld::ConstraintType::Unknown;
}

bool ConstraintFactory::GetConstraintBodies(PhysicsWorld* world, ConstraintHandle handle,
                                            RigidBodyHandle& bodyA, RigidBodyHandle& bodyB) {
    return world ? world->GetConstraintBodies(handle, bodyA, bodyB) : false;
}

} // namespace physics
} // namespace eyelib
