#include "CharacterController.h"
#include "api/animation/Animator.h"
#include "api/animation/AnimationClip.h"
#include "eyelib/math/Types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

namespace eyelib {
namespace test {

CharacterController::CharacterController() = default;

void CharacterController::SetPosition(const math::Vec3& position) {
    m_position = position;
}

void CharacterController::SetRotation(float yaw) {
    m_rotation = yaw;
    // Normalize rotation to 0-360 range
    while (m_rotation < 0.0f) m_rotation += 360.0f;
    while (m_rotation >= 360.0f) m_rotation -= 360.0f;
}

math::Vec3 CharacterController::GetForward() const {
    float yawRad = glm::radians(m_rotation);
    math::Vec3 forward;
    forward.x = std::cos(yawRad);
    forward.y = 0.0f;
    forward.z = std::sin(yawRad);
    return glm::normalize(forward);
}

math::Mat4 CharacterController::GetTransform() const {
    math::Mat4 transform(1.0f);
    transform = glm::translate(transform, m_position);
    // Add 180 degrees to rotation to fix model facing direction
    transform = glm::rotate(transform, glm::radians(m_rotation + 180.0f), math::Vec3(0.0f, 1.0f, 0.0f));
    // Scale model down by 50x (0.02 scale)
    transform = glm::scale(transform, math::Vec3(0.02f, 0.02f, 0.02f));
    return transform;
}

void CharacterController::UpdateMovement(const math::Vec2& moveInput, const math::Vec3& cameraForward, float deltaTime) {
    // Project camera forward onto horizontal plane
    math::Vec3 cameraForwardFlat = cameraForward;
    cameraForwardFlat.y = 0.0f;
    cameraForwardFlat = glm::normalize(cameraForwardFlat);
    
    // Calculate right direction
    math::Vec3 up(0.0f, 1.0f, 0.0f);
    math::Vec3 right = glm::normalize(glm::cross(cameraForwardFlat, up));
    
    // Calculate movement direction relative to camera
    math::Vec3 moveDirection = (cameraForwardFlat * moveInput.y) + (right * moveInput.x);
    moveDirection.y = 0.0f; // Keep movement on horizontal plane
    
    float moveLength = glm::length(moveDirection);
    if (moveLength > 0.001f) {
        moveDirection = glm::normalize(moveDirection);
        
        // Running state is set externally via SetRunning()
        float speed = m_isRunning ? m_runSpeed : m_walkSpeed;
        m_currentSpeed = speed;
        
        // Move character
        math::Vec3 movement = moveDirection * speed * deltaTime;
        m_position += movement;
        
        // Rotate character toward movement direction
        // atan2(x, z) gives angle where x=cos, z=sin (matching GetForward)
        float targetYaw = std::atan2(moveDirection.x, moveDirection.z) * 180.0f / 3.14159265f;
        // Ensure yaw is in 0-360 range
        if (targetYaw < 0.0f) targetYaw += 360.0f;
        
        // Smooth rotation toward target
        float rotationDiff = targetYaw - m_rotation;
        // Normalize rotation difference to -180 to 180 range
        while (rotationDiff > 180.0f) rotationDiff -= 360.0f;
        while (rotationDiff < -180.0f) rotationDiff += 360.0f;
        
        float maxRotation = m_rotationSpeed * deltaTime;
        if (std::abs(rotationDiff) > maxRotation) {
            m_rotation += (rotationDiff > 0.0f ? maxRotation : -maxRotation);
        } else {
            m_rotation = targetYaw;
        }
        
        // Normalize rotation
        while (m_rotation < 0.0f) m_rotation += 360.0f;
        while (m_rotation >= 360.0f) m_rotation -= 360.0f;
        
        // Update animation state
        m_animationState = m_isRunning ? AnimationState::Run : AnimationState::Walk;
    } else {
        m_currentSpeed = 0.0f;
        m_animationState = AnimationState::Idle;
    }
}

void CharacterController::UpdateAnimations(
    std::shared_ptr<api::Animator> animator,
    std::shared_ptr<api::AnimationClip> idleClip,
    std::shared_ptr<api::AnimationClip> walkClip,
    std::shared_ptr<api::AnimationClip> runClip) {
    
    if (!animator) return;
    
    // Switch animation clips based on state
    std::shared_ptr<api::AnimationClip> targetClip = idleClip;
    
    switch (m_animationState) {
        case AnimationState::Idle:
            targetClip = idleClip;
            break;
        case AnimationState::Walk:
            targetClip = walkClip ? walkClip : idleClip;
            break;
        case AnimationState::Run:
            targetClip = runClip ? runClip : (walkClip ? walkClip : idleClip);
            break;
    }
    
    // Only set animation clip if it actually changed (prevents resetting animation time)
    if (targetClip && targetClip != m_currentClip) {
        animator->SetClip(targetClip);
        m_currentClip = targetClip;
    }
}

void CharacterController::SetWalkSpeed(float speed) {
    m_walkSpeed = std::max(0.0f, speed);
}

void CharacterController::SetRunSpeed(float speed) {
    m_runSpeed = std::max(0.0f, speed);
}

void CharacterController::SetRotationSpeed(float speed) {
    m_rotationSpeed = std::max(0.0f, speed);
}

} // namespace test
} // namespace eyelib
