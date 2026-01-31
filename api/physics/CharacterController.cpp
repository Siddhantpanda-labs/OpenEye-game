#include "api/physics/CharacterController.h"

namespace eyelib {
namespace api {
namespace physics {

CharacterControllerHandle CharacterController::Create(PhysicsWorld& world, const eyelib::math::Vec3& position, float radius, float height) {
    auto* w = world.GetInternal();
    return w ? w->CreateCharacterController(position, radius, height) : 0;
}

void CharacterController::Destroy(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    if (w) w->DestroyCharacterController(handle);
}

void CharacterController::Move(PhysicsWorld& world, CharacterControllerHandle handle,
                               const eyelib::math::Vec3& direction, float deltaTime) {
    auto* w = world.GetInternal();
    if (w) w->MoveCharacterController(handle, direction, deltaTime);
}

void CharacterController::SetPosition(PhysicsWorld& world, CharacterControllerHandle handle, const eyelib::math::Vec3& position) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterPosition(handle, position);
}

eyelib::math::Vec3 CharacterController::GetPosition(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    return w ? w->GetCharacterPosition(handle) : eyelib::math::Vec3(0.f);
}

void CharacterController::SetRotation(PhysicsWorld& world, CharacterControllerHandle handle, const eyelib::math::Quat& rotation) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterRotation(handle, rotation);
}

eyelib::math::Quat CharacterController::GetRotation(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    return w ? w->GetCharacterRotation(handle) : eyelib::math::Quat(1.f, 0.f, 0.f, 0.f);
}

eyelib::math::Vec3 CharacterController::GetVelocity(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    return w ? w->GetCharacterVelocity(handle) : eyelib::math::Vec3(0.f);
}

bool CharacterController::IsOnGround(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    return w ? w->IsCharacterOnGround(handle) : false;
}

eyelib::math::Vec3 CharacterController::GetGroundNormal(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    return w ? w->GetCharacterGroundNormal(handle) : eyelib::math::Vec3(0.f, 1.f, 0.f);
}

void CharacterController::SetStepHeight(PhysicsWorld& world, CharacterControllerHandle handle, float height) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterStepHeight(handle, height);
}

void CharacterController::SetSlopeLimit(PhysicsWorld& world, CharacterControllerHandle handle, float radians) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterSlopeLimit(handle, radians);
}

void CharacterController::SetJumpSpeed(PhysicsWorld& world, CharacterControllerHandle handle, float speed) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterJumpSpeed(handle, speed);
}

void CharacterController::SetFallSpeed(PhysicsWorld& world, CharacterControllerHandle handle, float speed) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterFallSpeed(handle, speed);
}

void CharacterController::SetMaxJumpHeight(PhysicsWorld& world, CharacterControllerHandle handle, float height) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterMaxJumpHeight(handle, height);
}

void CharacterController::SetGravity(PhysicsWorld& world, CharacterControllerHandle handle, const eyelib::math::Vec3& gravity) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterGravity(handle, gravity);
}

void CharacterController::Jump(PhysicsWorld& world, CharacterControllerHandle handle) {
    auto* w = world.GetInternal();
    if (w) w->JumpCharacter(handle);
}

void CharacterController::SetCollisionFilter(PhysicsWorld& world, CharacterControllerHandle handle, short group, short mask) {
    auto* w = world.GetInternal();
    if (w) w->SetCharacterCollisionFilter(handle, group, mask);
}

} // namespace physics
} // namespace api
} // namespace eyelib
