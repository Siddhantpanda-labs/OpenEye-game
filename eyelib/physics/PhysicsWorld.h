#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class btDynamicsWorld;
class btCollisionObject;
class btRigidBody;
class btSoftBody;
struct btSoftBodyWorldInfo;

namespace eyelib {
namespace physics {

using RigidBodyHandle = uint32_t;

struct RigidBodyTransform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct DebugLine {
    math::Vec3 from{0.0f};
    math::Vec3 to{0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
};

class PhysicsWorld {
public:
    using ContactCallback = std::function<void(RigidBodyHandle, RigidBodyHandle, const math::Vec3&, const math::Vec3&)>;
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Initialize(const math::Vec3& gravity = math::Vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();

    void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f);

    RigidBodyHandle CreateBoxRigidBody(float mass, const math::Vec3& halfExtents, const math::Vec3& position);
    RigidBodyHandle CreateRigidBody(const math::Vec3& position, const math::Quat& rotation,
                                   CollisionShapeHandle shape, float mass = 0.0f,
                                   const math::Vec3& inertia = math::Vec3(0.0f));
    void DestroyRigidBody(RigidBodyHandle body);

    std::optional<RigidBodyTransform> GetRigidBodyTransform(RigidBodyHandle body) const;
    bool SetRigidBodyTransform(RigidBodyHandle body, const math::Vec3& position, const math::Quat& rotation);

    void SetVelocity(RigidBodyHandle body, const math::Vec3& velocity);
    math::Vec3 GetVelocity(RigidBodyHandle body) const;

    void SetAngularVelocity(RigidBodyHandle body, const math::Vec3& omega);
    math::Vec3 GetAngularVelocity(RigidBodyHandle body) const;

    void ApplyForce(RigidBodyHandle body, const math::Vec3& force, const math::Vec3& point);
    void ApplyImpulse(RigidBodyHandle body, const math::Vec3& impulse, const math::Vec3& point);

    void SetMass(RigidBodyHandle body, float mass);
    float GetMass(RigidBodyHandle body) const;

    void SetFriction(RigidBodyHandle body, float friction);
    void SetRestitution(RigidBodyHandle body, float restitution);

    void SetLinearDamping(RigidBodyHandle body, float damping);
    void SetAngularDamping(RigidBodyHandle body, float damping);

    void SetKinematic(RigidBodyHandle body, bool kinematic);
    bool IsKinematic(RigidBodyHandle body) const;

    void SetGravity(const math::Vec3& gravity);
    math::Vec3 GetGravity() const;

    void SetSolverIterations(int iterations);
    void SetSolverSubSteps(int subSteps);
    void SetContactCallback(std::function<void(RigidBodyHandle, RigidBodyHandle, const math::Vec3&, const math::Vec3&)> cb);
    void SetCollisionFilter(RigidBodyHandle body, short group, short mask);
    size_t GetBodyCount() const;
    size_t GetConstraintCount() const;
    void CollectDebugDrawData();
    const std::vector<DebugLine>& GetDebugDrawData() const;

    using ConstraintHandle = uint32_t;
    ConstraintHandle CreateHingeConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB,
        const math::Vec3& axisA, const math::Vec3& axisB);
    ConstraintHandle CreateBallSocketConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB);
    ConstraintHandle CreateSliderConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    ConstraintHandle CreateFixedConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    ConstraintHandle CreateSpringConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB, float stiffness, float damping);
    ConstraintHandle Create6DOFConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Mat4& frameA, const math::Mat4& frameB);
    ConstraintHandle CreateGearConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& axisA, const math::Vec3& axisB, float ratio);
    ConstraintHandle CreateRopeConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
        const math::Vec3& pivotA, const math::Vec3& pivotB, float length);
    void DestroyConstraint(ConstraintHandle h);
    void SetHingeLimits(ConstraintHandle h, float lower, float upper);
    void SetHingeMotor(ConstraintHandle h, bool enabled, float targetVelocity, float maxForce);

    using CharacterControllerHandle = uint32_t;
    CharacterControllerHandle CreateCharacterController(const math::Vec3& position, float radius, float height);
    void DestroyCharacterController(CharacterControllerHandle h);
    void MoveCharacterController(CharacterControllerHandle h, const math::Vec3& direction, float deltaTime);
    void SetCharacterPosition(CharacterControllerHandle h, const math::Vec3& position);
    math::Vec3 GetCharacterPosition(CharacterControllerHandle h) const;
    void SetCharacterRotation(CharacterControllerHandle h, const math::Quat& rotation);
    math::Quat GetCharacterRotation(CharacterControllerHandle h) const;
    bool IsCharacterOnGround(CharacterControllerHandle h) const;
    math::Vec3 GetCharacterGroundNormal(CharacterControllerHandle h) const;
    void SetCharacterStepHeight(CharacterControllerHandle h, float height);
    void SetCharacterSlopeLimit(CharacterControllerHandle h, float radians);
    void SetCharacterCollisionFilter(CharacterControllerHandle h, short group, short mask);

    btDynamicsWorld* GetWorldInternal();
    RigidBodyHandle GetHandleFromCollisionObject(const btCollisionObject* obj) const;
    btRigidBody* GetRigidBodyInternal(RigidBodyHandle body) const;

    using SoftBodyHandle = uint32_t;
    SoftBodyHandle AddSoftBody(btSoftBody* body);
    void RemoveSoftBody(SoftBodyHandle body);
    bool HasSoftBody(SoftBodyHandle body) const;
    btSoftBodyWorldInfo& GetSoftBodyWorldInfo();
    btSoftBody* GetSoftBodyInternal(SoftBodyHandle body) const;

private:
    class Impl;
    Impl* m_impl = nullptr;

    friend class RigidBodyFactory;
    friend class ConstraintFactory;
    friend class CharacterController;
    friend class PhysicsQuery;
};

} // namespace physics
} // namespace eyelib

