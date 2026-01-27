#pragma once

#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace api {
namespace render {
class Camera;
}
}

namespace test {

/**
 * @brief Free-flying camera controller.
 * 
 * Camera moves freely in 3D space.
 * Mouse controls camera rotation (yaw/pitch).
 * WASD + QE for movement.
 */
class ThirdPersonCamera {
public:
    ThirdPersonCamera();
    ~ThirdPersonCamera() = default;


    /**
     * @brief Update camera with mouse input for look around.
     * 
     * @param mouseDelta Mouse delta movement (x = yaw, y = pitch)
     */
    void UpdateInput(const math::Vec2& mouseDelta);

    /**
     * @brief Update camera movement (WASD + QE).
     * 
     * @param moveInput Movement input (x = right/left, y = forward/back, z = up/down)
     * @param deltaTime Frame delta time
     */
    void UpdateMovement(const math::Vec3& moveInput, float deltaTime);

    /**
     * @brief Update camera position and rotation.
     * Should be called every frame.
     */
    void Update(float deltaTime);

    /**
     * @brief Get camera view matrix.
     */
    math::Mat4 GetViewMatrix() const;

    /**
     * @brief Get camera position.
     */
    math::Vec3 GetPosition() const { return m_position; }

    /**
     * @brief Get camera forward direction.
     */
    math::Vec3 GetForward() const;

    /**
     * @brief Get camera right direction.
     */
    math::Vec3 GetRight() const;

    /**
     * @brief Set camera movement speed.
     */
    void SetMoveSpeed(float speed);

    /**
     * @brief Get camera movement speed.
     */
    float GetMoveSpeed() const { return m_moveSpeed; }

    /**
     * @brief Set mouse sensitivity for orbit control.
     */
    void SetSensitivity(float sensitivity);

    /**
     * @brief Get mouse sensitivity.
     */
    float GetSensitivity() const { return m_sensitivity; }

    /**
     * @brief Set camera rotation limits.
     * 
     * @param minPitch Minimum pitch angle in degrees (default: -89)
     * @param maxPitch Maximum pitch angle in degrees (default: 89)
     */
    void SetPitchLimits(float minPitch, float maxPitch);

private:
    math::Vec3 m_position{0.0f, 2.0f, 5.0f};
    
    float m_yaw = -90.0f;      // Horizontal rotation (degrees)
    float m_pitch = 0.0f;      // Vertical rotation (degrees)
    
    float m_moveSpeed = 5.0f;          // Movement speed
    float m_sensitivity = 0.15f;       // Mouse sensitivity
    float m_minPitch = -89.0f;          // Minimum pitch angle
    float m_maxPitch = 89.0f;           // Maximum pitch angle
};

} // namespace test
} // namespace eyelib
