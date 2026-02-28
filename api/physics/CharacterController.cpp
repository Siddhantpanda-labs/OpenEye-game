#include "api/physics/CharacterController.h"

namespace eyelib {
namespace api {
namespace physics {

CharacterControllerHandle CharacterController::Create(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*position*/, float /*radius*/, float /*height*/) {
    return 0;
}

void CharacterController::Destroy(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {}

void CharacterController::Move(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/,
                               const eyelib::math::Vec3& /*direction*/, float /*deltaTime*/) {}

void CharacterController::SetPosition(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, const eyelib::math::Vec3& /*position*/) {}

eyelib::math::Vec3 CharacterController::GetPosition(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {
    return eyelib::math::Vec3(0.f);
}

void CharacterController::SetRotation(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, const eyelib::math::Quat& /*rotation*/) {}

eyelib::math::Quat CharacterController::GetRotation(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {
    return eyelib::math::Quat(1.f, 0.f, 0.f, 0.f);
}

eyelib::math::Vec3 CharacterController::GetVelocity(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {
    return eyelib::math::Vec3(0.f);
}

bool CharacterController::IsOnGround(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {
    return false;
}

eyelib::math::Vec3 CharacterController::GetGroundNormal(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {
    return eyelib::math::Vec3(0.f, 1.f, 0.f);
}

void CharacterController::SetStepHeight(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, float /*height*/) {}

void CharacterController::SetSlopeLimit(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, float /*radians*/) {}

void CharacterController::SetJumpSpeed(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, float /*speed*/) {}

void CharacterController::SetFallSpeed(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, float /*speed*/) {}

void CharacterController::SetMaxJumpHeight(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, float /*height*/) {}

void CharacterController::SetGravity(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, const eyelib::math::Vec3& /*gravity*/) {}

void CharacterController::Jump(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/) {}

void CharacterController::SetCollisionFilter(PhysicsWorld& /*world*/, CharacterControllerHandle /*handle*/, short /*group*/, short /*mask*/) {}

} // namespace physics
} // namespace api
} // namespace eyelib
