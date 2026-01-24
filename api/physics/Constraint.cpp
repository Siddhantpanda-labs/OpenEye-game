#include "api/physics/Constraint.h"

namespace eyelib {
namespace api {

ConstraintHandle Constraint::CreateHinge(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateHinge(w, bodyA, bodyB, pivotA, pivotB, axisA, axisB) : 0;
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

ConstraintHandle Constraint::CreateFixed(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::ConstraintFactory::CreateFixed(w, bodyA, bodyB, frameA, frameB) : 0;
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

} // namespace api
} // namespace eyelib
