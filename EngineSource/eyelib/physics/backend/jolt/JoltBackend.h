#pragma once

#include "api/physics/IPhysicsWorld.h"

#include <vector>

namespace eyelib {
namespace physics {

/**
 * Physics backend implementing api::physics::IPhysicsWorld.
 * Thin facade: delegates to JoltShapeStore (shapes) and JoltBodyStore (bodies + integration).
 * Add JoltCollision, JoltQueries, etc. under backend/jolt/ as the module grows.
 */
class JoltBackend : public eyelib::api::physics::IPhysicsWorld {
public:
    JoltBackend();
    ~JoltBackend() override;

    bool Initialize(const eyelib::math::Vec3& gravity = eyelib::math::Vec3(0.0f, -9.81f, 0.0f)) override;
    void Shutdown() override;

    void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f) override;

    void SetGravity(const eyelib::math::Vec3& gravity) override;
    eyelib::math::Vec3 GetGravity() const override;

    eyelib::api::physics::CollisionShapeHandle CreateBoxShape(const eyelib::math::Vec3& halfExtents) override;
    eyelib::api::physics::CollisionShapeHandle CreateSphereShape(float radius) override;
    eyelib::api::physics::CollisionShapeHandle CreateCapsuleShape(float radius, float height) override;
    void DestroyShape(eyelib::api::physics::CollisionShapeHandle shape) override;

    eyelib::api::physics::RigidBodyHandle CreateRigidBody(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                                    eyelib::api::physics::CollisionShapeHandle shape, float mass = 0.0f,
                                    const eyelib::math::Vec3& inertia = eyelib::math::Vec3(0.0f)) override;
    void DestroyRigidBody(eyelib::api::physics::RigidBodyHandle body) override;

    std::optional<eyelib::api::physics::RigidBodyTransform> GetRigidBodyTransform(eyelib::api::physics::RigidBodyHandle body) const override;
    bool SetRigidBodyTransform(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) override;

    void SetVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& velocity) override;
    eyelib::math::Vec3 GetVelocity(eyelib::api::physics::RigidBodyHandle body) const override;
    void SetAngularVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& omega) override;
    eyelib::math::Vec3 GetAngularVelocity(eyelib::api::physics::RigidBodyHandle body) const override;

    void ApplyForce(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& force, const eyelib::math::Vec3& point) override;
    void ApplyImpulse(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& point) override;

    void SetMass(eyelib::api::physics::RigidBodyHandle body, float mass) override;
    float GetMass(eyelib::api::physics::RigidBodyHandle body) const override;
    void SetFriction(eyelib::api::physics::RigidBodyHandle body, float friction) override;
    void SetRestitution(eyelib::api::physics::RigidBodyHandle body, float restitution) override;
    void SetLinearDamping(eyelib::api::physics::RigidBodyHandle body, float damping) override;
    void SetAngularDamping(eyelib::api::physics::RigidBodyHandle body, float damping) override;

    void SetKinematic(eyelib::api::physics::RigidBodyHandle body, bool kinematic) override;
    bool IsKinematic(eyelib::api::physics::RigidBodyHandle body) const override;
    void SetRigidBodyGravity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& gravity) override;

    void SetCollisionFilter(eyelib::api::physics::RigidBodyHandle body, short group, short mask) override;

    void SetContactCallback(eyelib::api::physics::ContactCallback cb) override;

    size_t GetBodyCount() const override;
    size_t GetConstraintCount() const override;

    /** True if Shutdown() has not been called; backend is alive. */
    bool IsAlive() const { return m_impl != nullptr; }

    eyelib::api::physics::RaycastResult Raycast(const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) const override;
    std::vector<eyelib::api::physics::RigidBodyHandle> QueryAABB(const eyelib::math::AABB& aabb) const override;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace physics
} // namespace eyelib
