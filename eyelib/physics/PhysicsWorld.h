#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <optional>

namespace eyelib {
namespace physics {

using RigidBodyHandle = uint32_t;

struct RigidBodyTransform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Bullet-backed physics world (implementation hidden behind handles).
 *
 * Bullet types are kept out of headers. This keeps compile times lower and
 * prevents Bullet from leaking into API consumers.
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool Initialize(const math::Vec3& gravity = math::Vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();

    void Step(float deltaTime, int maxSubSteps = 3, float fixedTimeStep = 1.0f / 60.0f);

    // Rigid bodies (minimal set for bring-up)
    RigidBodyHandle CreateBoxRigidBody(float mass, const math::Vec3& halfExtents, const math::Vec3& position);
    void DestroyRigidBody(RigidBodyHandle body);

    std::optional<RigidBodyTransform> GetRigidBodyTransform(RigidBodyHandle body) const;
    bool SetRigidBodyTransform(RigidBodyHandle body, const math::Vec3& position, const math::Quat& rotation);

    void SetGravity(const math::Vec3& gravity);
    math::Vec3 GetGravity() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace physics
} // namespace eyelib

