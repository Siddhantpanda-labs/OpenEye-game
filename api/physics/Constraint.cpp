#include "api/physics/Constraint.h"

namespace eyelib {
namespace api {
namespace physics {

ConstraintHandle Constraint::CreateHinge(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateHinge(w, bodyA, bodyB, pivotA, pivotB, axisA, axisB) : 0;
}

ConstraintHandle Constraint::CreateHingeWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Vec3& pivot, const eyelib::math::Vec3& axis) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateHingeWorld(w, body, pivot, axis) : 0;
}

ConstraintHandle Constraint::CreateBallSocket(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateBallSocket(w, bodyA, bodyB, pivotA, pivotB) : 0;
}

ConstraintHandle Constraint::CreateSlider(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateSlider(w, bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle Constraint::CreateSliderWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateSliderWorld(w, body, frame) : 0;
}

ConstraintHandle Constraint::CreateFixed(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateFixed(w, bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle Constraint::CreateFixedWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateFixedWorld(w, body, frame) : 0;
}

ConstraintHandle Constraint::CreateSpring(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    float stiffness, float damping) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateSpring(w, bodyA, bodyB, pivotA, pivotB, stiffness, damping) : 0;
}

ConstraintHandle Constraint::Create6DOF(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::Create6DOF(w, bodyA, bodyB, frameA, frameB) : 0;
}

ConstraintHandle Constraint::Create6DOFWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::Create6DOFWorld(w, body, frame) : 0;
}

ConstraintHandle Constraint::CreateGear(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB,
    float ratio) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateGear(w, bodyA, bodyB, axisA, axisB, ratio) : 0;
}

ConstraintHandle Constraint::CreateRope(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    float length) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateRope(w, bodyA, bodyB, pivotA, pivotB, length) : 0;
}

void Constraint::Destroy(PhysicsWorld& world, ConstraintHandle handle) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::DestroyConstraint(w, handle);
}

void Constraint::SetHingeLimits(PhysicsWorld& world, ConstraintHandle handle, float lower, float upper) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::SetHingeLimits(w, handle, lower, upper);
}

void Constraint::SetHingeMotor(PhysicsWorld& world, ConstraintHandle handle,
                               bool enabled, float targetVelocity, float maxForce) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::SetHingeMotor(w, handle, enabled, targetVelocity, maxForce);
}

void Constraint::SetSliderLimits(PhysicsWorld& world, ConstraintHandle handle,
                                 float lowerLin, float upperLin, float lowerAng, float upperAng) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::SetSliderLimits(w, handle, lowerLin, upperLin, lowerAng, upperAng);
}

void Constraint::SetSliderMotor(PhysicsWorld& world, ConstraintHandle handle,
                                bool linearEnabled, float targetVelocity, float maxForce,
                                bool angularEnabled, float targetAngVelocity, float maxAngForce) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::SetSliderMotor(w, handle, linearEnabled, targetVelocity, maxForce,
                                                             angularEnabled, targetAngVelocity, maxAngForce);
}

void Constraint::Set6DOFLimits(PhysicsWorld& world, ConstraintHandle handle,
                               const eyelib::math::Vec3& linearLower, const eyelib::math::Vec3& linearUpper,
                               const eyelib::math::Vec3& angularLower, const eyelib::math::Vec3& angularUpper) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::Set6DOFLimits(w, handle, linearLower, linearUpper, angularLower, angularUpper);
}

void Constraint::Set6DOFSpring(PhysicsWorld& world, ConstraintHandle handle,
                               int axis, bool enabled, float stiffness, float damping) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::Set6DOFSpring(w, handle, axis, enabled, stiffness, damping);
}

void Constraint::Set6DOFEquilibrium(PhysicsWorld& world, ConstraintHandle handle, int axis) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::Set6DOFEquilibrium(w, handle, axis);
}

void Constraint::SetBreakingImpulseThreshold(PhysicsWorld& world, ConstraintHandle handle, float threshold) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::ConstraintFactory::SetBreakingImpulseThreshold(w, handle, threshold);
}

ConstraintType Constraint::GetConstraintType(PhysicsWorld& world, ConstraintHandle handle) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::GetConstraintType(w, handle)
             : eyelib::physics::PhysicsWorld::ConstraintType::Unknown;
}

bool Constraint::GetConstraintBodies(PhysicsWorld& world, ConstraintHandle handle,
                                     RigidBodyHandle& bodyA, RigidBodyHandle& bodyB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::GetConstraintBodies(w, handle, bodyA, bodyB) : false;
}

} // namespace physics
} // namespace api
} // namespace eyelib
