#pragma once

#include "api/physics/IPhysicsWorld.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eyelib {
namespace api {
namespace physics {

struct DebugLine {
    eyelib::math::Vec3 from{0.0f};
    eyelib::math::Vec3 to{0.0f};
    eyelib::math::Vec3 color{1.0f, 1.0f, 1.0f};
};

/**
 * @brief Client-facing physics world (no Bullet types exposed).
 */
class PhysicsWorld {
public:
    enum class ContactObjectType : uint8_t { Unknown = 0, RigidBody, SoftBody, CharacterController };
    struct ContactObject {
        ContactObjectType type = ContactObjectType::Unknown;
        uint32_t handle = 0;
    };
    using ContactCallbackEx = std::function<void(const ContactObject&, const ContactObject&,
                                                 const eyelib::math::Vec3&, const eyelib::math::Vec3&)>;

    enum class PhysicsBackendType { Jolt };

    PhysicsWorld();
    explicit PhysicsWorld(PhysicsBackendType backendType);
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Initialize(const eyelib::math::Vec3& gravity = eyelib::math::Vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();

    void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f);

    CollisionShapeHandle CreateBoxShape(const eyelib::math::Vec3& halfExtents);
    CollisionShapeHandle CreateSphereShape(float radius);
    CollisionShapeHandle CreateCapsuleShape(float radius, float height);
    void DestroyShape(CollisionShapeHandle shape);

    RigidBodyHandle CreateBoxRigidBody(float mass, const eyelib::math::Vec3& halfExtents, const eyelib::math::Vec3& position);
    RigidBodyHandle CreateRigidBody(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                                    CollisionShapeHandle shape, float mass = 0.0f,
                                    const eyelib::math::Vec3& inertia = eyelib::math::Vec3(0.0f));
    void DestroyRigidBody(RigidBodyHandle body);

    std::optional<RigidBodyTransform> GetRigidBodyTransform(RigidBodyHandle body) const;
    bool SetRigidBodyTransform(RigidBodyHandle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation);

    void SetGravity(const eyelib::math::Vec3& gravity);
    eyelib::math::Vec3 GetGravity() const;

    void SetSolverIterations(int iterations);
    void SetSolverSubSteps(int subSteps);
    void SetCollisionFilter(RigidBodyHandle body, short group, short mask);
    size_t GetBodyCount() const;
    size_t GetConstraintCount() const;

    void SetContactCallback(std::function<void(RigidBodyHandle, RigidBodyHandle, const eyelib::math::Vec3&, const eyelib::math::Vec3&)> cb);
    void SetContactCallbackEx(ContactCallbackEx cb);

    void CollectDebugDrawData();
    const std::vector<DebugLine>& GetDebugDrawData() const;

private:
    class Impl;
    Impl* m_impl = nullptr;

    IPhysicsWorld* GetInternal();
    const IPhysicsWorld* GetInternal() const;

    friend class CollisionShape;
    friend class RigidBody;
    friend class Constraint;
    friend class CharacterController;
    friend class Query;
    friend class SoftBody;
    friend class Vehicle;
};

} // namespace physics
} // namespace api
} // namespace eyelib

