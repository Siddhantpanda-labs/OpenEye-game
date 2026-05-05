#include "api/physics/Constraint.h"

namespace eyelib {
namespace api {
namespace physics {

#define STUB_RETURN_0 \
    (void)world; return 0;
#define STUB_RETURN_VOID \
    (void)world; (void)handle;
#define STUB_RETURN_UNKNOWN \
    (void)world; (void)handle; return ConstraintType::Unknown;
#define STUB_RETURN_FALSE \
    (void)world; (void)handle; (void)bodyA; (void)bodyB; return false;

ConstraintHandle Constraint::CreateHinge(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB) {
    (void)bodyA; (void)bodyB; (void)pivotA; (void)pivotB; (void)axisA; (void)axisB;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateHingeWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Vec3& pivot, const eyelib::math::Vec3& axis) {
    (void)body; (void)pivot; (void)axis;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateBallSocket(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB) {
    (void)bodyA; (void)bodyB; (void)pivotA; (void)pivotB;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateSlider(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    (void)bodyA; (void)bodyB; (void)frameA; (void)frameB;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateSliderWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    (void)body; (void)frame;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateFixed(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    (void)bodyA; (void)bodyB; (void)frameA; (void)frameB;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateFixedWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    (void)body; (void)frame;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateSpring(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    float stiffness, float damping) {
    (void)bodyA; (void)bodyB; (void)pivotA; (void)pivotB; (void)stiffness; (void)damping;
    STUB_RETURN_0
}

ConstraintHandle Constraint::Create6DOF(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Mat4& frameA, const eyelib::math::Mat4& frameB) {
    (void)bodyA; (void)bodyB; (void)frameA; (void)frameB;
    STUB_RETURN_0
}

ConstraintHandle Constraint::Create6DOFWorld(PhysicsWorld& world,
    RigidBodyHandle body, const eyelib::math::Mat4& frame) {
    (void)body; (void)frame;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateGear(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& axisA, const eyelib::math::Vec3& axisB,
    float ratio) {
    (void)bodyA; (void)bodyB; (void)axisA; (void)axisB; (void)ratio;
    STUB_RETURN_0
}

ConstraintHandle Constraint::CreateRope(PhysicsWorld& world,
    RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const eyelib::math::Vec3& pivotA, const eyelib::math::Vec3& pivotB,
    float length) {
    (void)bodyA; (void)bodyB; (void)pivotA; (void)pivotB; (void)length;
    STUB_RETURN_0
}

void Constraint::Destroy(PhysicsWorld& world, ConstraintHandle handle) { STUB_RETURN_VOID }

void Constraint::SetHingeLimits(PhysicsWorld& world, ConstraintHandle handle, float lower, float upper) {
    (void)lower; (void)upper; STUB_RETURN_VOID
}

void Constraint::SetHingeMotor(PhysicsWorld& world, ConstraintHandle handle,
                               bool enabled, float targetVelocity, float maxForce) {
    (void)enabled; (void)targetVelocity; (void)maxForce; STUB_RETURN_VOID
}

void Constraint::SetSliderLimits(PhysicsWorld& world, ConstraintHandle handle,
                                 float lowerLin, float upperLin, float lowerAng, float upperAng) {
    (void)lowerLin; (void)upperLin; (void)lowerAng; (void)upperAng; STUB_RETURN_VOID
}

void Constraint::SetSliderMotor(PhysicsWorld& world, ConstraintHandle handle,
                                bool linearEnabled, float targetVelocity, float maxForce,
                                bool angularEnabled, float targetAngVelocity, float maxAngForce) {
    (void)linearEnabled; (void)targetVelocity; (void)maxForce;
    (void)angularEnabled; (void)targetAngVelocity; (void)maxAngForce;
    STUB_RETURN_VOID
}

void Constraint::Set6DOFLimits(PhysicsWorld& world, ConstraintHandle handle,
                               const eyelib::math::Vec3& linearLower, const eyelib::math::Vec3& linearUpper,
                               const eyelib::math::Vec3& angularLower, const eyelib::math::Vec3& angularUpper) {
    (void)linearLower; (void)linearUpper; (void)angularLower; (void)angularUpper;
    STUB_RETURN_VOID
}

void Constraint::Set6DOFSpring(PhysicsWorld& world, ConstraintHandle handle,
                               int axis, bool enabled, float stiffness, float damping) {
    (void)axis; (void)enabled; (void)stiffness; (void)damping;
    STUB_RETURN_VOID
}

void Constraint::Set6DOFEquilibrium(PhysicsWorld& world, ConstraintHandle handle, int axis) {
    (void)axis; STUB_RETURN_VOID
}

void Constraint::SetBreakingImpulseThreshold(PhysicsWorld& world, ConstraintHandle handle, float threshold) {
    (void)threshold; STUB_RETURN_VOID
}

ConstraintType Constraint::GetConstraintType(PhysicsWorld& world, ConstraintHandle handle) {
    STUB_RETURN_UNKNOWN
}

bool Constraint::GetConstraintBodies(PhysicsWorld& world, ConstraintHandle handle,
                                    RigidBodyHandle& bodyA, RigidBodyHandle& bodyB) {
    STUB_RETURN_FALSE
}

#undef STUB_RETURN_0
#undef STUB_RETURN_VOID
#undef STUB_RETURN_UNKNOWN
#undef STUB_RETURN_FALSE

} // namespace physics
} // namespace api
} // namespace eyelib
