#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"

namespace eyelib {
namespace physics {

/**
 * @brief Factory for creating and managing rigid bodies. Uses shape handles,
 * delegates to PhysicsWorld. No Bullet types exposed.
 */
class RigidBodyFactory {
public:
    /** Create rigid body. Returns 0 on failure. */
    static RigidBodyHandle Create(
        PhysicsWorld* world,
        const math::Vec3& position,
        const math::Quat& rotation,
        CollisionShapeHandle shape,
        float mass = 0.0f,
        const math::Vec3& inertia = math::Vec3(0.0f));

    static void Destroy(PhysicsWorld* world, RigidBodyHandle body);

    static bool SetTransform(PhysicsWorld* world, RigidBodyHandle body,
                            const math::Vec3& position, const math::Quat& rotation);
    static bool GetTransform(PhysicsWorld* world, RigidBodyHandle body,
                            math::Vec3& position, math::Quat& rotation);

    static void SetVelocity(PhysicsWorld* world, RigidBodyHandle body, const math::Vec3& velocity);
    static math::Vec3 GetVelocity(PhysicsWorld* world, RigidBodyHandle body);

    static void SetAngularVelocity(PhysicsWorld* world, RigidBodyHandle body, const math::Vec3& omega);
    static math::Vec3 GetAngularVelocity(PhysicsWorld* world, RigidBodyHandle body);

    static void ApplyForce(PhysicsWorld* world, RigidBodyHandle body,
                          const math::Vec3& force, const math::Vec3& point);
    static void ApplyImpulse(PhysicsWorld* world, RigidBodyHandle body,
                            const math::Vec3& impulse, const math::Vec3& point);

    static void SetMass(PhysicsWorld* world, RigidBodyHandle body, float mass);
    static float GetMass(PhysicsWorld* world, RigidBodyHandle body);

    static void SetFriction(PhysicsWorld* world, RigidBodyHandle body, float friction);
    static void SetRestitution(PhysicsWorld* world, RigidBodyHandle body, float restitution);

    static void SetLinearDamping(PhysicsWorld* world, RigidBodyHandle body, float damping);
    static void SetAngularDamping(PhysicsWorld* world, RigidBodyHandle body, float damping);

    static void SetKinematic(PhysicsWorld* world, RigidBodyHandle body, bool kinematic);
    static bool IsKinematic(PhysicsWorld* world, RigidBodyHandle body);
};

} // namespace physics
} // namespace eyelib
