#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eyelib {
namespace api {
namespace physics {

using RigidBodyHandle = uint32_t;
using CollisionShapeHandle = uint32_t;

struct RigidBodyTransform {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

using ContactCallback = std::function<void(RigidBodyHandle, RigidBodyHandle, const eyelib::math::Vec3&, const eyelib::math::Vec3&)>;

struct RaycastResult {
    bool hit = false;
    eyelib::math::Vec3 point{0.0f};
    eyelib::math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    RigidBodyHandle body = 0;
};

/**
 * Physics world interface. Implemented by backends (e.g. Jolt).
 * Used only within api layer and by impl; client code uses PhysicsWorld, not this.
 */
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    virtual bool Initialize(const eyelib::math::Vec3& gravity = eyelib::math::Vec3(0.0f, -9.81f, 0.0f)) = 0;
    virtual void Shutdown() = 0;
    virtual void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f) = 0;

    virtual void SetGravity(const eyelib::math::Vec3& gravity) = 0;
    virtual eyelib::math::Vec3 GetGravity() const = 0;

    virtual CollisionShapeHandle CreateBoxShape(const eyelib::math::Vec3& halfExtents) = 0;
    virtual CollisionShapeHandle CreateSphereShape(float radius) = 0;
    virtual CollisionShapeHandle CreateCapsuleShape(float radius, float height) = 0;
    virtual void DestroyShape(CollisionShapeHandle shape) = 0;

    virtual RigidBodyHandle CreateRigidBody(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                                            CollisionShapeHandle shape, float mass = 0.0f,
                                            const eyelib::math::Vec3& inertia = eyelib::math::Vec3(0.0f)) = 0;
    virtual void DestroyRigidBody(RigidBodyHandle body) = 0;

    virtual std::optional<RigidBodyTransform> GetRigidBodyTransform(RigidBodyHandle body) const = 0;
    virtual bool SetRigidBodyTransform(RigidBodyHandle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) = 0;

    virtual void SetVelocity(RigidBodyHandle body, const eyelib::math::Vec3& velocity) = 0;
    virtual eyelib::math::Vec3 GetVelocity(RigidBodyHandle body) const = 0;
    virtual void SetAngularVelocity(RigidBodyHandle body, const eyelib::math::Vec3& omega) = 0;
    virtual eyelib::math::Vec3 GetAngularVelocity(RigidBodyHandle body) const = 0;

    virtual void ApplyForce(RigidBodyHandle body, const eyelib::math::Vec3& force, const eyelib::math::Vec3& point) = 0;
    virtual void ApplyImpulse(RigidBodyHandle body, const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& point) = 0;

    virtual void SetMass(RigidBodyHandle body, float mass) = 0;
    virtual float GetMass(RigidBodyHandle body) const = 0;
    virtual void SetFriction(RigidBodyHandle body, float friction) = 0;
    virtual void SetRestitution(RigidBodyHandle body, float restitution) = 0;
    virtual void SetLinearDamping(RigidBodyHandle body, float damping) = 0;
    virtual void SetAngularDamping(RigidBodyHandle body, float damping) = 0;

    virtual void SetKinematic(RigidBodyHandle body, bool kinematic) = 0;
    virtual bool IsKinematic(RigidBodyHandle body) const = 0;
    virtual void SetRigidBodyGravity(RigidBodyHandle body, const eyelib::math::Vec3& gravity) = 0;

    virtual void SetCollisionFilter(RigidBodyHandle body, short group, short mask) = 0;
    virtual void SetContactCallback(ContactCallback cb) = 0;

    virtual size_t GetBodyCount() const = 0;
    virtual size_t GetConstraintCount() const = 0;

    virtual RaycastResult Raycast(const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) const = 0;
    virtual std::vector<RigidBodyHandle> QueryAABB(const eyelib::math::AABB& aabb) const = 0;
};

} // namespace physics
} // namespace api
} // namespace eyelib
