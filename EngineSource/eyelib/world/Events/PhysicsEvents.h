#pragma once

#include "eyelib/world/EntityId.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace world {

/** How to apply a force: Force = scale by 1/mass (F=ma); Acceleration = add directly to velocity (ignores mass). */
enum class ForceMode { Force, Acceleration };

/** Queued force/impulse entries; applied at start of each substep by PhysicsSystem. */
struct PendingForceEntry { EntityId entity = InvalidEntity; math::Vec3 force{0.f, 0.f, 0.f}; ForceMode mode = ForceMode::Force; };
struct PendingImpulseEntry { EntityId entity = InvalidEntity; math::Vec3 impulse{0.f, 0.f, 0.f}; };
struct PendingTorqueEntry { EntityId entity = InvalidEntity; math::Vec3 torque{0.f, 0.f, 0.f}; };
struct PendingAngularImpulseEntry { EntityId entity = InvalidEntity; math::Vec3 impulse{0.f, 0.f, 0.f}; };

/** Per-substep contact. Enqueued by CollisionResponseSystem; includes optional narrow-phase detail (normal, depth, point). */
struct ContactEvent {
    EntityId entityA = InvalidEntity;
    EntityId entityB = InvalidEntity;
    /** Contact normal from A toward B. Filled by narrow phase or AABB fallback. */
    math::Vec3 normal{0.f, 0.f, 0.f};
    /** Penetration depth (positive when overlapping). */
    float penetrationDepth = 0.f;
    /** World-space contact point (e.g. on surface of A or midpoint). */
    math::Vec3 contactPoint{0.f, 0.f, 0.f};
};

} // namespace world
} // namespace eyelib
