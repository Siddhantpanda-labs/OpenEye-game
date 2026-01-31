#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace physics {

using CharacterControllerHandle = PhysicsWorld::CharacterControllerHandle;

/**
 * @brief Capsule-based character controller. Uses handles and delegates to PhysicsWorld.
 */
class CharacterController {
public:
    static CharacterControllerHandle Create(PhysicsWorld* world, const math::Vec3& position, float radius, float height);
    static void Destroy(PhysicsWorld* world, CharacterControllerHandle handle);

    static void Move(PhysicsWorld* world, CharacterControllerHandle handle,
                     const math::Vec3& direction, float deltaTime);

    static void SetPosition(PhysicsWorld* world, CharacterControllerHandle handle, const math::Vec3& position);
    static math::Vec3 GetPosition(PhysicsWorld* world, CharacterControllerHandle handle);

    static void SetRotation(PhysicsWorld* world, CharacterControllerHandle handle, const math::Quat& rotation);
    static math::Quat GetRotation(PhysicsWorld* world, CharacterControllerHandle handle);
    static math::Vec3 GetVelocity(PhysicsWorld* world, CharacterControllerHandle handle);

    static bool IsOnGround(PhysicsWorld* world, CharacterControllerHandle handle);
    static math::Vec3 GetGroundNormal(PhysicsWorld* world, CharacterControllerHandle handle);

    static void SetStepHeight(PhysicsWorld* world, CharacterControllerHandle handle, float height);
    static void SetSlopeLimit(PhysicsWorld* world, CharacterControllerHandle handle, float radians);
    static void SetJumpSpeed(PhysicsWorld* world, CharacterControllerHandle handle, float speed);
    static void SetFallSpeed(PhysicsWorld* world, CharacterControllerHandle handle, float speed);
    static void SetMaxJumpHeight(PhysicsWorld* world, CharacterControllerHandle handle, float height);
    static void SetGravity(PhysicsWorld* world, CharacterControllerHandle handle, const math::Vec3& gravity);
    static void Jump(PhysicsWorld* world, CharacterControllerHandle handle);
    static void SetCollisionFilter(PhysicsWorld* world, CharacterControllerHandle handle, short group, short mask);
};

} // namespace physics
} // namespace eyelib
