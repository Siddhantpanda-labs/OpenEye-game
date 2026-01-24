#pragma once

#include "api/physics/PhysicsWorld.h"
#include "api/physics/CollisionShape.h"

namespace eyelib {
namespace api {

/**
 * @brief Rigid body API (opaque handles).
 */
class RigidBody {
public:
    static RigidBodyHandle Create(PhysicsWorld& world,
                                  const eyelib::math::Vec3& position,
                                  const eyelib::math::Quat& rotation,
                                  CollisionShapeHandle shape,
                                  float mass = 0.0f,
                                  const eyelib::math::Vec3& inertia = eyelib::math::Vec3(0.0f));

    static void Destroy(PhysicsWorld& world, RigidBodyHandle body);

    static bool SetTransform(PhysicsWorld& world, RigidBodyHandle body,
                             const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation);
    static std::optional<RigidBodyTransform> GetTransform(PhysicsWorld& world, RigidBodyHandle body);

    static void SetVelocity(PhysicsWorld& world, RigidBodyHandle body, const eyelib::math::Vec3& velocity);
    static eyelib::math::Vec3 GetVelocity(PhysicsWorld& world, RigidBodyHandle body);

    static void SetAngularVelocity(PhysicsWorld& world, RigidBodyHandle body, const eyelib::math::Vec3& omega);
    static eyelib::math::Vec3 GetAngularVelocity(PhysicsWorld& world, RigidBodyHandle body);

    static void ApplyForce(PhysicsWorld& world, RigidBodyHandle body,
                           const eyelib::math::Vec3& force, const eyelib::math::Vec3& point);
    static void ApplyImpulse(PhysicsWorld& world, RigidBodyHandle body,
                             const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& point);

    static void SetMass(PhysicsWorld& world, RigidBodyHandle body, float mass);
    static float GetMass(PhysicsWorld& world, RigidBodyHandle body);

    static void SetFriction(PhysicsWorld& world, RigidBodyHandle body, float friction);
    static void SetRestitution(PhysicsWorld& world, RigidBodyHandle body, float restitution);

    static void SetLinearDamping(PhysicsWorld& world, RigidBodyHandle body, float damping);
    static void SetAngularDamping(PhysicsWorld& world, RigidBodyHandle body, float damping);

    static void SetKinematic(PhysicsWorld& world, RigidBodyHandle body, bool kinematic);
    static bool IsKinematic(PhysicsWorld& world, RigidBodyHandle body);
};

} // namespace api
} // namespace eyelib
