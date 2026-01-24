#include "eyelib/physics/bodies/RigidBodyFactory.h"
#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace physics {

RigidBodyHandle RigidBodyFactory::Create(
    PhysicsWorld* world,
    const math::Vec3& position,
    const math::Quat& rotation,
    CollisionShapeHandle shape,
    float mass,
    const math::Vec3& inertia) {
    if (!world) return 0;
    return world->CreateRigidBody(position, rotation, shape, mass, inertia);
}

void RigidBodyFactory::Destroy(PhysicsWorld* world, RigidBodyHandle body) {
    if (world) world->DestroyRigidBody(body);
}

bool RigidBodyFactory::SetTransform(PhysicsWorld* world, RigidBodyHandle body,
                                   const math::Vec3& position, const math::Quat& rotation) {
    return world ? world->SetRigidBodyTransform(body, position, rotation) : false;
}

bool RigidBodyFactory::GetTransform(PhysicsWorld* world, RigidBodyHandle body,
                                   math::Vec3& position, math::Quat& rotation) {
    if (!world) return false;
    auto opt = world->GetRigidBodyTransform(body);
    if (!opt) return false;
    position = opt->position;
    rotation = opt->rotation;
    return true;
}

void RigidBodyFactory::SetVelocity(PhysicsWorld* world, RigidBodyHandle body, const math::Vec3& velocity) {
    if (world) world->SetVelocity(body, velocity);
}

math::Vec3 RigidBodyFactory::GetVelocity(PhysicsWorld* world, RigidBodyHandle body) {
    return world ? world->GetVelocity(body) : math::Vec3(0.f);
}

void RigidBodyFactory::SetAngularVelocity(PhysicsWorld* world, RigidBodyHandle body, const math::Vec3& omega) {
    if (world) world->SetAngularVelocity(body, omega);
}

math::Vec3 RigidBodyFactory::GetAngularVelocity(PhysicsWorld* world, RigidBodyHandle body) {
    return world ? world->GetAngularVelocity(body) : math::Vec3(0.f);
}

void RigidBodyFactory::ApplyForce(PhysicsWorld* world, RigidBodyHandle body,
                                 const math::Vec3& force, const math::Vec3& point) {
    if (world) world->ApplyForce(body, force, point);
}

void RigidBodyFactory::ApplyImpulse(PhysicsWorld* world, RigidBodyHandle body,
                                   const math::Vec3& impulse, const math::Vec3& point) {
    if (world) world->ApplyImpulse(body, impulse, point);
}

void RigidBodyFactory::SetMass(PhysicsWorld* world, RigidBodyHandle body, float mass) {
    if (world) world->SetMass(body, mass);
}

float RigidBodyFactory::GetMass(PhysicsWorld* world, RigidBodyHandle body) {
    return world ? world->GetMass(body) : 0.f;
}

void RigidBodyFactory::SetFriction(PhysicsWorld* world, RigidBodyHandle body, float friction) {
    if (world) world->SetFriction(body, friction);
}

void RigidBodyFactory::SetRestitution(PhysicsWorld* world, RigidBodyHandle body, float restitution) {
    if (world) world->SetRestitution(body, restitution);
}

void RigidBodyFactory::SetLinearDamping(PhysicsWorld* world, RigidBodyHandle body, float damping) {
    if (world) world->SetLinearDamping(body, damping);
}

void RigidBodyFactory::SetAngularDamping(PhysicsWorld* world, RigidBodyHandle body, float damping) {
    if (world) world->SetAngularDamping(body, damping);
}

void RigidBodyFactory::SetKinematic(PhysicsWorld* world, RigidBodyHandle body, bool kinematic) {
    if (world) world->SetKinematic(body, kinematic);
}

bool RigidBodyFactory::IsKinematic(PhysicsWorld* world, RigidBodyHandle body) {
    return world ? world->IsKinematic(body) : false;
}

} // namespace physics
} // namespace eyelib
