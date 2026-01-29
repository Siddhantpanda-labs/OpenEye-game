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
    transform = glm::rotate(transform, glm::radians(m_rotation), math::Vec3(0.0f, 1.0f, 0.0f));
    // Scale model down by 50x (0.02 scale)
    transform = glm::scale(transform, math::Vec3(0.02f, 0.02f, 0.02f));
    return transform;
}

void CharacterController::UpdateMovement(const math::Vec2& moveInput, const math::Vec3& cameraForward, float deltaTime,
                                        api::Animator::MotionAuthority motionAuthority) {
    // Update animation state based on input
    // Movement is controlled by root motion, not direct input
    if (glm::length(moveInput) > 0.001f) {
        m_animationState = m_isRunning ? AnimationState::Run : AnimationState::Walk;
        m_currentSpeed = m_isRunning ? m_runSpeed : m_walkSpeed;
        
        // Calculate movement direction for character rotation (root motion controls actual movement)
        math::Vec3 cameraForwardFlat = cameraForward;
        cameraForwardFlat.y = 0.0f;
        cameraForwardFlat = glm::normalize(cameraForwardFlat);
        
        math::Vec3 up(0.0f, 1.0f, 0.0f);
        math::Vec3 right = glm::normalize(glm::cross(cameraForwardFlat, up));
        
        math::Vec3 moveDirection = (cameraForwardFlat * moveInput.y) + (right * moveInput.x);
        moveDirection.y = 0.0f;
        moveDirection = glm::normalize(moveDirection);
        
        // Rotate character toward movement direction
        float targetYaw = std::atan2(moveDirection.x, moveDirection.z) * 180.0f / 3.14159265f;
        
        // Smooth rotation toward target
        float rotationDiff = targetYaw - m_rotation;
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
    } else {
        m_currentSpeed = 0.0f;
        m_animationState = AnimationState::Idle;
    }
    
    // No direct input-based movement - root motion controls all movement
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

void CharacterController::ApplyRootMotion(const api::Animator::RootMotionDelta& rootMotionDelta,
                                         api::Animator::MotionAuthority motionAuthority) {
    if (!rootMotionDelta.hasMotion) return;
    
    // Skip root motion if motion is controller-driven
    if (motionAuthority == api::Animator::MotionAuthority::ControllerDriven) {
        return;
    }
    
    // Apply root motion translation to character position
    // Root motion is in local space (character's forward/right/up), so transform by character rotation
    float yawRad = glm::radians(m_rotation);
    math::Mat4 rotationMat(1.0f);
    rotationMat = glm::rotate(rotationMat, yawRad, math::Vec3(0.0f, 1.0f, 0.0f));
    
    // Transform root motion translation from local to world space
    math::Vec4 motionWorld = rotationMat * math::Vec4(rootMotionDelta.translation, 0.0f);
    
    // Apply root motion (AnimationDriven or Hybrid - both use root motion)
    m_position += math::Vec3(motionWorld.x, motionWorld.y, motionWorld.z);
}

} // namespace test
} // namespace eyelib
