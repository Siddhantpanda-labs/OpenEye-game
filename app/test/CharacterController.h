#pragma once

#include "eyelib/math/Types.h"
#include "api/animation/AnimationClip.h"
#include "api/animation/Animator.h"
#include <memory>

namespace eyelib {
namespace test {

/**
 * @brief Character movement controller.
 * 
 * Handles WASD movement relative to camera direction.
 * Character rotates toward movement direction.
 * Manages animation states (idle/walk/run) based on movement speed.
 */
class CharacterController {
public:
    CharacterController();
    ~CharacterController() = default;

    /**
     * @brief Set character position.
     */
    void SetPosition(const math::Vec3& position);

    /**
     * @brief Get character position.
     */
    math::Vec3 GetPosition() const { return m_position; }

    /**
     * @brief Set character rotation (yaw in degrees).
     */
    void SetRotation(float yaw);

    /**
     * @brief Get character rotation (yaw in degrees).
     */
    float GetRotation() const { return m_rotation; }

    /**
     * @brief Get character forward direction.
     */
    math::Vec3 GetForward() const;

    /**
     * @brief Get character transform matrix.
     */
    math::Mat4 GetTransform() const;

    /**
     * @brief Update character movement based on input.
     * 
     * Only applies movement if motion authority is ControllerDriven or Hybrid.
     * 
     * @param moveInput Movement input (-1 to 1 for forward/back, -1 to 1 for left/right)
     * @param cameraForward Camera forward direction (for relative movement)
     * @param deltaTime Frame delta time
     * @param motionAuthority Current motion authority mode
     */
    void UpdateMovement(const math::Vec2& moveInput, const math::Vec3& cameraForward, float deltaTime, 
                       api::Animator::MotionAuthority motionAuthority = api::Animator::MotionAuthority::ControllerDriven);

    /**
     * @brief Update character animations based on current state.
     * 
     * @param animator Animator to control
     * @param idleClip Idle animation clip
     * @param walkClip Walk animation clip (optional)
     * @param runClip Run animation clip (optional)
     */
    void UpdateAnimations(
        std::shared_ptr<api::Animator> animator,
        std::shared_ptr<api::AnimationClip> idleClip,
        std::shared_ptr<api::AnimationClip> walkClip = nullptr,
        std::shared_ptr<api::AnimationClip> runClip = nullptr);

    /**
     * @brief Apply root motion delta to character transform.
     * 
     * Only applies root motion if motion authority is AnimationDriven or Hybrid.
     * 
     * @param rootMotionDelta Root motion delta from animator
     * @param motionAuthority Current motion authority mode
     */
    void ApplyRootMotion(const api::Animator::RootMotionDelta& rootMotionDelta,
                        api::Animator::MotionAuthority motionAuthority = api::Animator::MotionAuthority::AnimationDriven);

    /**
     * @brief Set movement speed.
     */
    void SetWalkSpeed(float speed);

    /**
     * @brief Get walk speed.
     */
    float GetWalkSpeed() const { return m_walkSpeed; }

    /**
     * @brief Set run speed.
     */
    void SetRunSpeed(float speed);

    /**
     * @brief Get run speed.
     */
    float GetRunSpeed() const { return m_runSpeed; }

    /**
     * @brief Set rotation speed (degrees per second).
     */
    void SetRotationSpeed(float speed);

    /**
     * @brief Get rotation speed.
     */
    float GetRotationSpeed() const { return m_rotationSpeed; }

    /**
     * @brief Get current movement speed (for animation blending).
     */
    float GetCurrentSpeed() const { return m_currentSpeed; }

    /**
     * @brief Check if character is moving.
     */
    bool IsMoving() const { return m_currentSpeed > 0.01f; }

    /**
     * @brief Set running state.
     */
    void SetRunning(bool running) { m_isRunning = running; }

private:
    math::Vec3 m_position{0.0f, 0.0f, 0.0f};
    float m_rotation = 0.0f;  // Yaw rotation in degrees
    
    float m_walkSpeed = 3.0f;
    float m_runSpeed = 6.0f;
    float m_rotationSpeed = 360.0f;  // Degrees per second
    
    float m_currentSpeed = 0.0f;
    bool m_isRunning = false;
    
    // Animation state
    enum class AnimationState {
        Idle,
        Walk,
        Run
    };
    AnimationState m_animationState = AnimationState::Idle;
    
    // Track current animation clip to avoid resetting time unnecessarily
    std::shared_ptr<api::AnimationClip> m_currentClip;
};

} // namespace test
} // namespace eyelib
