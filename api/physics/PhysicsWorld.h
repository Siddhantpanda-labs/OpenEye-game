#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <optional>

namespace eyelib {
namespace api {

using RigidBodyHandle = uint32_t;

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

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

