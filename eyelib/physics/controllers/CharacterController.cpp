#include "eyelib/physics/controllers/CharacterController.h"

namespace eyelib {
namespace physics {

CharacterControllerHandle CharacterController::Create(PhysicsWorld* world, const math::Vec3& position, float radius, float height) {
    return world ? world->CreateCharacterController(position, radius, height) : 0;
}

void CharacterController::Destroy(PhysicsWorld* world, CharacterControllerHandle handle) {
    if (world) world->DestroyCharacterController(handle);
}

void CharacterController::Move(PhysicsWorld* world, CharacterControllerHandle handle,
                               const math::Vec3& direction, float deltaTime) {
    if (world) world->MoveCharacterController(handle, direction, deltaTime);
}

void CharacterController::SetPosition(PhysicsWorld* world, CharacterControllerHandle handle, const math::Vec3& position) {
    if (world) world->SetCharacterPosition(handle, position);
}

math::Vec3 CharacterController::GetPosition(PhysicsWorld* world, CharacterControllerHandle handle) {
    return world ? world->GetCharacterPosition(handle) : math::Vec3(0.f);
}

void CharacterController::SetRotation(PhysicsWorld* world, CharacterControllerHandle handle, const math::Quat& rotation) {
    if (world) world->SetCharacterRotation(handle, rotation);
}

math::Quat CharacterController::GetRotation(PhysicsWorld* world, CharacterControllerHandle handle) {
    return world ? world->GetCharacterRotation(handle) : math::Quat(1.f, 0.f, 0.f, 0.f);
}

math::Vec3 CharacterController::GetVelocity(PhysicsWorld* world, CharacterControllerHandle handle) {
    return world ? world->GetCharacterVelocity(handle) : math::Vec3(0.f);
}

bool CharacterController::IsOnGround(PhysicsWorld* world, CharacterControllerHandle handle) {
    return world ? world->IsCharacterOnGround(handle) : false;
}

math::Vec3 CharacterController::GetGroundNormal(PhysicsWorld* world, CharacterControllerHandle handle) {
    return world ? world->GetCharacterGroundNormal(handle) : math::Vec3(0.f, 1.f, 0.f);
}

void CharacterController::SetStepHeight(PhysicsWorld* world, CharacterControllerHandle handle, float height) {
    if (world) world->SetCharacterStepHeight(handle, height);
}

void CharacterController::SetSlopeLimit(PhysicsWorld* world, CharacterControllerHandle handle, float radians) {
    if (world) world->SetCharacterSlopeLimit(handle, radians);
}

void CharacterController::SetJumpSpeed(PhysicsWorld* world, CharacterControllerHandle handle, float speed) {
    if (world) world->SetCharacterJumpSpeed(handle, speed);
}

void CharacterController::SetFallSpeed(PhysicsWorld* world, CharacterControllerHandle handle, float speed) {
    if (world) world->SetCharacterFallSpeed(handle, speed);
}

void CharacterController::SetMaxJumpHeight(PhysicsWorld* world, CharacterControllerHandle handle, float height) {
    if (world) world->SetCharacterMaxJumpHeight(handle, height);
}

void CharacterController::SetGravity(PhysicsWorld* world, CharacterControllerHandle handle, const math::Vec3& gravity) {
    if (world) world->SetCharacterGravity(handle, gravity);
}

void CharacterController::Jump(PhysicsWorld* world, CharacterControllerHandle handle) {
    if (world) world->JumpCharacter(handle);
}

void CharacterController::SetCollisionFilter(PhysicsWorld* world, CharacterControllerHandle handle, short group, short mask) {
    if (world) world->SetCharacterCollisionFilter(handle, group, mask);
}

} // namespace physics
} // namespace eyelib
