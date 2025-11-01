#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace OpenEye {
namespace Rendering {

class Camera {
public:
    Camera();
    
    // Set position and target
    void SetPosition(const glm::vec3& position);
    void SetTarget(const glm::vec3& target);
    
    // Get matrices
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    
    // Projection settings
    void SetPerspective(float fov, float aspect, float nearPlane, float farPlane);
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    
    // Camera properties
    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetForward() const { return m_forward; }
    glm::vec3 GetRight() const { return m_right; }
    glm::vec3 GetUp() const { return m_up; }
    
    // Update camera (call every frame)
    void Update(float deltaTime);
    
    // Input handling
    void ProcessKeyboard(int key, bool pressed);
    void ProcessMouseMovement(float deltaX, float deltaY, bool rightButtonDown);
    void ProcessMouseScroll(float deltaY);
    
    // Movement settings
    void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void SetScrollSensitivity(float sensitivity) { m_scrollSensitivity = sensitivity; }
    
    // Get settings
    float GetMovementSpeed() const { return m_movementSpeed; }
    float GetMouseSensitivity() const { return m_mouseSensitivity; }
    float GetFOV() const { return m_fov; }

private:
    void UpdateVectors();
    void UpdateViewMatrix() const;
    
    // Position and orientation
    glm::vec3 m_position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 m_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Rotation
    float m_yaw = -90.0f;   // Rotation around Y axis (left/right)
    float m_pitch = 0.0f;   // Rotation around X axis (up/down)
    
    // Movement state
    bool m_forwardPressed = false;
    bool m_backwardPressed = false;
    bool m_leftPressed = false;
    bool m_rightPressed = false;
    bool m_upPressed = false;
    bool m_downPressed = false;
    
    // Projection
    bool m_isPerspective = true;
    float m_fov = 45.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
    
    // Orthographic
    float m_orthoLeft = -10.0f;
    float m_orthoRight = 10.0f;
    float m_orthoBottom = -10.0f;
    float m_orthoTop = 10.0f;
    
    // Settings
    float m_movementSpeed = 5.0f;
    float m_mouseSensitivity = 0.1f;
    float m_scrollSensitivity = 2.0f;
    
    // Cached matrices (mutable for lazy evaluation in const methods)
    mutable glm::mat4 m_viewMatrix;
    mutable glm::mat4 m_projectionMatrix;
    mutable bool m_viewMatrixDirty = true;
    mutable bool m_projectionMatrixDirty = true;
};

} // namespace Rendering
} // namespace OpenEye

