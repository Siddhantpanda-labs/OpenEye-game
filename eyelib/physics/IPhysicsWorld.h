#pragma once

#include "eyelib/physics/PhysicsTypes.h"

namespace eyelib {
namespace physics {

/**
 * Physics world interface. Engine and api talk only to this.
 * Implemented by backend/jolt (and later backend/physx). No backend types in this header.
 */
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    virtual bool Initialize(const math::Vec3& gravity = math::Vec3(0.0f, -9.81f, 0.0f)) = 0;
    virtual void Shutdown() = 0;

    virtual void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f) = 0;

    // World gravity
    virtual void SetGravity(const math::Vec3& gravity) = 0;
    virtual math::Vec3 GetGravity() const = 0;

    // Shapes (backend-owned; create before bodies)
    virtual CollisionShapeHandle CreateBoxShape(const math::Vec3& halfExtents) = 0;
    virtual CollisionShapeHandle CreateSphereShape(float radius) = 0;
    virtual CollisionShapeHandle CreateCapsuleShape(float radius, float height) = 0;
    virtual void DestroyShape(CollisionShapeHandle shape) = 0;

    // Rigid bodies
    virtual RigidBodyHandle CreateRigidBody(const math::Vec3& position, const math::Quat& rotation,
                                            CollisionShapeHandle shape, float mass = 0.0f,
                                            const math::Vec3& inertia = math::Vec3(0.0f)) = 0;
    virtual void DestroyRigidBody(RigidBodyHandle body) = 0;

    virtual std::optional<RigidBodyTransform> GetRigidBodyTransform(RigidBodyHandle body) const = 0;
    virtual bool SetRigidBodyTransform(RigidBodyHandle body, const math::Vec3& position, const math::Quat& rotation) = 0;

    virtual void SetVelocity(RigidBodyHandle body, const math::Vec3& velocity) = 0;
    virtual math::Vec3 GetVelocity(RigidBodyHandle body) const = 0;
    virtual void SetAngularVelocity(RigidBodyHandle body, const math::Vec3& omega) = 0;
    virtual math::Vec3 GetAngularVelocity(RigidBodyHandle body) const = 0;

    virtual void ApplyForce(RigidBodyHandle body, const math::Vec3& force, const math::Vec3& point) = 0;
    virtual void ApplyImpulse(RigidBodyHandle body, const math::Vec3& impulse, const math::Vec3& point) = 0;

    virtual void SetMass(RigidBodyHandle body, float mass) = 0;
    virtual float GetMass(RigidBodyHandle body) const = 0;
    virtual void SetFriction(RigidBodyHandle body, float friction) = 0;
    virtual void SetRestitution(RigidBodyHandle body, float restitution) = 0;
    virtual void SetLinearDamping(RigidBodyHandle body, float damping) = 0;
    virtual void SetAngularDamping(RigidBodyHandle body, float damping) = 0;

    virtual void SetKinematic(RigidBodyHandle body, bool kinematic) = 0;
    virtual bool IsKinematic(RigidBodyHandle body) const = 0;
    virtual void SetRigidBodyGravity(RigidBodyHandle body, const math::Vec3& gravity) = 0;

    virtual void SetCollisionFilter(RigidBodyHandle body, short group, short mask) = 0;

    virtual void SetContactCallback(ContactCallback cb) = 0;
    virtual void SetContactCallbackEx(ContactCallbackEx cb) = 0;

    virtual size_t GetBodyCount() const = 0;
    virtual size_t GetConstraintCount() const = 0;

    virtual void CollectDebugDrawData() = 0;
    virtual const std::vector<DebugLine>& GetDebugDrawData() const = 0;
};

} // namespace physics
} // namespace eyelib
