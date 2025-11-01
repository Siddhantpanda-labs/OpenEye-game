#include "Camera.h"
#include "core/log/Log.h"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace OpenEye {
namespace Rendering {

Camera::Camera() {
    UpdateVectors();
}

void Camera::SetPosition(const glm::vec3& position) {
    m_position = position;
    m_viewMatrixDirty = true;
}

void Camera::SetTarget(const glm::vec3& target) {
    glm::vec3 direction = glm::normalize(target - m_position);
    m_pitch = glm::degrees(asin(direction.y));
    m_yaw = glm::degrees(atan2(direction.z, direction.x));
    m_viewMatrixDirty = true;
    UpdateVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    if (m_viewMatrixDirty) {
        UpdateViewMatrix();
        m_viewMatrixDirty = false;
    }
    return m_viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix() const {
    if (m_projectionMatrixDirty) {
        if (m_isPerspective) {
            m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspect, m_nearPlane, m_farPlane);
        } else {
            m_projectionMatrix = glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_nearPlane, m_farPlane);
        }
        m_projectionMatrixDirty = false;
    }
    return m_projectionMatrix;
}

void Camera::SetPerspective(float fov, float aspect, float nearPlane, float farPlane) {
    m_isPerspective = true;
    m_fov = fov;
    m_aspect = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_projectionMatrixDirty = true;
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_isPerspective = false;
    m_orthoLeft = left;
    m_orthoRight = right;
    m_orthoBottom = bottom;
    m_orthoTop = top;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_projectionMatrixDirty = true;
}

void Camera::Update(float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;
    
    // Forward/backward movement
    if (m_forwardPressed) {
        m_position += m_forward * velocity;
        m_viewMatrixDirty = true;
    }
    if (m_backwardPressed) {
        m_position -= m_forward * velocity;
        m_viewMatrixDirty = true;
    }
    
    // Left/right movement (strafe)
    if (m_leftPressed) {
        m_position -= m_right * velocity;
        m_viewMatrixDirty = true;
    }
    if (m_rightPressed) {
        m_position += m_right * velocity;
        m_viewMatrixDirty = true;
    }
    
    // Up/down movement
    if (m_upPressed) {
        m_position += m_worldUp * velocity;
        m_viewMatrixDirty = true;
    }
    if (m_downPressed) {
        m_position -= m_worldUp * velocity;
        m_viewMatrixDirty = true;
    }
}

void Camera::ProcessKeyboard(int key, bool pressed) {
    if (key == GLFW_KEY_W || key == GLFW_KEY_UP) {
        m_forwardPressed = pressed;
    } else if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) {
        m_backwardPressed = pressed;
    } else if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) {
        m_leftPressed = pressed;
    } else if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) {
        m_rightPressed = pressed;
    } else if (key == GLFW_KEY_Q) {
        m_downPressed = pressed;
    } else if (key == GLFW_KEY_E) {
        m_upPressed = pressed;
    }
}

void Camera::ProcessMouseMovement(float deltaX, float deltaY, bool rightButtonDown) {
    if (!rightButtonDown) {
        return;
    }
    
    deltaX *= m_mouseSensitivity;
    deltaY *= m_mouseSensitivity;
    
    m_yaw += deltaX;
    m_pitch -= deltaY; // Reversed since y-coordinates range from bottom to top
    
    // Constrain pitch to prevent gimbal lock
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    
    // Update forward, right, and up vectors
    UpdateVectors();
    m_viewMatrixDirty = true;
}

void Camera::ProcessMouseScroll(float deltaY) {
    // Zoom in/out by changing FOV
    m_fov -= deltaY * m_scrollSensitivity;
    m_fov = std::clamp(m_fov, 1.0f, 120.0f);
    m_projectionMatrixDirty = true;
}

void Camera::UpdateVectors() {
    // Calculate new forward vector
    glm::vec3 forward;
    forward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    forward.y = sin(glm::radians(m_pitch));
    forward.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward = glm::normalize(forward);
    
    // Recalculate right and up vectors
    m_right = glm::normalize(glm::cross(m_forward, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

void Camera::UpdateViewMatrix() const {
    m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, m_up);
}

} // namespace Rendering
} // namespace OpenEye

