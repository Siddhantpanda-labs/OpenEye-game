#include "api/physics/RigidBody.h"
#include "api/physics/IPhysicsWorld.h"

namespace eyelib {
namespace api {
namespace physics {

RigidBodyHandle RigidBody::Create(PhysicsWorld& world,
                                  const eyelib::math::Vec3& position,
                                  const eyelib::math::Quat& rotation,
                                  CollisionShapeHandle shape,
                                  float mass,
                                  const eyelib::math::Vec3& inertia) {
    auto* w = world.GetInternal();
    return w ? w->CreateRigidBody(position, rotation, shape, mass, inertia) : 0;
}

void RigidBody::Destroy(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    if (w) w->DestroyRigidBody(body);
}

bool RigidBody::SetTransform(PhysicsWorld& world, RigidBodyHandle body,
                             const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    auto* w = world.GetInternal();
    return w ? w->SetRigidBodyTransform(body, position, rotation) : false;
}

std::optional<RigidBodyTransform> RigidBody::GetTransform(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    if (!w) return std::nullopt;
    auto t = w->GetRigidBodyTransform(body);
    if (!t) return std::nullopt;
    return RigidBodyTransform{ t->position, t->rotation };
}

void RigidBody::SetVelocity(PhysicsWorld& world, RigidBodyHandle body, const eyelib::math::Vec3& velocity) {
    auto* w = world.GetInternal();
    if (w) w->SetVelocity(body, velocity);
}

eyelib::math::Vec3 RigidBody::GetVelocity(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    return w ? w->GetVelocity(body) : eyelib::math::Vec3(0.f);
}

void RigidBody::SetAngularVelocity(PhysicsWorld& world, RigidBodyHandle body, const eyelib::math::Vec3& omega) {
    auto* w = world.GetInternal();
    if (w) w->SetAngularVelocity(body, omega);
}

eyelib::math::Vec3 RigidBody::GetAngularVelocity(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    return w ? w->GetAngularVelocity(body) : eyelib::math::Vec3(0.f);
}

void RigidBody::ApplyForce(PhysicsWorld& world, RigidBodyHandle body,
                           const eyelib::math::Vec3& force, const eyelib::math::Vec3& point) {
    auto* w = world.GetInternal();
    if (w) w->ApplyForce(body, force, point);
}

void RigidBody::ApplyImpulse(PhysicsWorld& world, RigidBodyHandle body,
                             const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& point) {
    auto* w = world.GetInternal();
    if (w) w->ApplyImpulse(body, impulse, point);
}

void RigidBody::SetMass(PhysicsWorld& world, RigidBodyHandle body, float mass) {
    auto* w = world.GetInternal();
    if (w) w->SetMass(body, mass);
}

float RigidBody::GetMass(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    return w ? w->GetMass(body) : 0.f;
}

void RigidBody::SetFriction(PhysicsWorld& world, RigidBodyHandle body, float friction) {
    auto* w = world.GetInternal();
    if (w) w->SetFriction(body, friction);
}

void RigidBody::SetRestitution(PhysicsWorld& world, RigidBodyHandle body, float restitution) {
    auto* w = world.GetInternal();
    if (w) w->SetRestitution(body, restitution);
}

void RigidBody::SetLinearDamping(PhysicsWorld& world, RigidBodyHandle body, float damping) {
    auto* w = world.GetInternal();
    if (w) w->SetLinearDamping(body, damping);
}

void RigidBody::SetAngularDamping(PhysicsWorld& world, RigidBodyHandle body, float damping) {
    auto* w = world.GetInternal();
    if (w) w->SetAngularDamping(body, damping);
}

void RigidBody::SetKinematic(PhysicsWorld& world, RigidBodyHandle body, bool kinematic) {
    auto* w = world.GetInternal();
    if (w) w->SetKinematic(body, kinematic);
}

bool RigidBody::IsKinematic(PhysicsWorld& world, RigidBodyHandle body) {
    auto* w = world.GetInternal();
    return w ? w->IsKinematic(body) : false;
}

void RigidBody::SetGravity(PhysicsWorld& world, RigidBodyHandle body, const eyelib::math::Vec3& gravity) {
    auto* w = world.GetInternal();
    if (w) w->SetRigidBodyGravity(body, gravity);
}

} // namespace physics
} // namespace api
} // namespace eyelib
