#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eyelib {
namespace physics { class PhysicsWorld; }
namespace api {
namespace physics {

using RigidBodyHandle = uint32_t;

struct DebugLine {
    eyelib::math::Vec3 from{0.0f};
    eyelib::math::Vec3 to{0.0f};
    eyelib::math::Vec3 color{1.0f, 1.0f, 1.0f};
};

struct RigidBodyTransform {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Client-facing physics world (no Bullet types exposed).
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Initialize(const eyelib::math::Vec3& gravity = eyelib::math::Vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();

    void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f);

    RigidBodyHandle CreateBoxRigidBody(float mass, const eyelib::math::Vec3& halfExtents, const eyelib::math::Vec3& position);
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

    void CollectDebugDrawData();
    const std::vector<DebugLine>& GetDebugDrawData() const;

private:
    class Impl;
    Impl* m_impl = nullptr;

    ::eyelib::physics::PhysicsWorld* GetInternal();
    const ::eyelib::physics::PhysicsWorld* GetInternal() const;

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

