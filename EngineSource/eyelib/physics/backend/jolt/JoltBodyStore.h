#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eyelib {
namespace physics {

/**
 * Rigid body storage and integration for the Jolt backend.
 * Owns body state (transform, velocity, mass, etc.) and Step() integration.
 * Internal to backend/jolt; not part of the public API.
 */
class JoltBodyStore {
public:
    using Handle = uint32_t;

    struct Transform {
        eyelib::math::Vec3 position{0.0f};
        eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    JoltBodyStore() = default;

    /** Create a body. shapeHandle is opaque (validated by backend). Returns 0 on failure. */
    Handle Create(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                  uint32_t shapeHandle, float mass);
    void Destroy(Handle body);

    std::optional<Transform> GetTransform(Handle body) const;
    bool SetTransform(Handle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation);

    eyelib::math::Vec3 GetVelocity(Handle body) const;
    void SetVelocity(Handle body, const eyelib::math::Vec3& velocity);
    eyelib::math::Vec3 GetAngularVelocity(Handle body) const;
    void SetAngularVelocity(Handle body, const eyelib::math::Vec3& omega);

    void ApplyForce(Handle body, const eyelib::math::Vec3& force, const eyelib::math::Vec3& point);
    void ApplyImpulse(Handle body, const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& point);

    void SetMass(Handle body, float mass);
    float GetMass(Handle body) const;
    void SetLinearDamping(Handle body, float damping);
    void SetAngularDamping(Handle body, float damping);
    void SetKinematic(Handle body, bool kinematic);
    bool IsKinematic(Handle body) const;

    /** Integrate dynamics: apply forces, gravity, update position and rotation. */
    void Step(const eyelib::math::Vec3& gravity, float deltaTime);

    size_t GetAliveCount() const;

    uint32_t GetShapeHandle(Handle body) const;

    /** Call visitor(handle, transform, shapeHandle) for each alive body. */
    void VisitAliveBodies(std::function<void(Handle, const Transform&, uint32_t shapeHandle)> visitor) const;

    void Clear();

private:
    struct Entry {
        eyelib::math::Vec3 position{0.0f};
        eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        eyelib::math::Vec3 linearVelocity{0.0f};
        eyelib::math::Vec3 angularVelocity{0.0f};
        eyelib::math::Vec3 accumulatedForce{0.0f};
        float mass = 1.0f;
        bool kinematic = false;
        bool useGravity = true;
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;
        uint32_t shapeHandle = 0;
        bool alive = false;
    };

    std::vector<Entry> m_entries;

    static Handle ToHandle(size_t oneBased);
    static size_t ToIndex(Handle h);
    Entry* Get(Handle body);
    const Entry* Get(Handle body) const;
};

} // namespace physics
} // namespace eyelib
