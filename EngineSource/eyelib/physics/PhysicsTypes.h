#pragma once

#include "eyelib/physics/PhysicsHandles.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eyelib {
namespace physics {

using RigidBodyHandle = PhysicsBodyId;
using CollisionShapeHandle = PhysicsShapeId;
using ConstraintHandle = uint32_t;
using CharacterControllerHandle = uint32_t;
using SoftBodyHandle = uint32_t;

struct RigidBodyTransform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct DebugLine {
    math::Vec3 from{0.0f};
    math::Vec3 to{0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
};

enum class ContactObjectType : uint8_t { Unknown = 0, RigidBody, SoftBody, CharacterController };

struct ContactObject {
    ContactObjectType type = ContactObjectType::Unknown;
    uint32_t handle = 0;
};

using ContactCallback = std::function<void(RigidBodyHandle, RigidBodyHandle, const math::Vec3&, const math::Vec3&)>;
using ContactCallbackEx = std::function<void(const ContactObject&, const ContactObject&,
                                             const math::Vec3&, const math::Vec3&)>;

} // namespace physics
} // namespace eyelib
