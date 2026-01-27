#include "ThirdPersonCamera.h"
#include "eyelib/math/Types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace test {

ThirdPersonCamera::ThirdPersonCamera() = default;

void ThirdPersonCamera::UpdateInput(const math::Vec2& mouseDelta) {
    // Update yaw and pitch from mouse input
    m_yaw += mouseDelta.x * m_sensitivity;
    m_pitch -= mouseDelta.y * m_sensitivity;
    
    // Clamp pitch to prevent camera flipping
    m_pitch = std::clamp(m_pitch, m_minPitch, m_maxPitch);
}

void ThirdPersonCamera::UpdateMovement(const math::Vec3& moveInput, float deltaTime) {
    // Get camera directions
    math::Vec3 forward = GetForward();
    math::Vec3 right = GetRight();
    math::Vec3 up(0.0f, 1.0f, 0.0f);
    
    // Project forward onto horizontal plane for movement
    math::Vec3 forwardFlat = forward;
    forwardFlat.y = 0.0f;
    forwardFlat = glm::normalize(forwardFlat);
    
    // Calculate movement direction
    math::Vec3 movement = (forwardFlat * moveInput.y) + (right * moveInput.x) + (up * moveInput.z);
    
    // Apply movement
    m_position += movement * m_moveSpeed * deltaTime;
}

void ThirdPersonCamera::Update(float deltaTime) {
    // Camera position is updated via UpdateMovement
    // This method is kept for compatibility but does nothing
}

math::Mat4 ThirdPersonCamera::GetViewMatrix() const {
    math::Vec3 forward = GetForward();
    math::Vec3 up(0.0f, 1.0f, 0.0f);
    return glm::lookAt(m_position, m_position + forward, up);
}

math::Vec3 ThirdPersonCamera::GetForward() const {
    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);
    
    math::Vec3 forward;
    forward.x = std::cos(pitchRad) * std::cos(yawRad);
    forward.y = std::sin(pitchRad);
    forward.z = std::cos(pitchRad) * std::sin(yawRad);
    
    return glm::normalize(forward);
}

math::Vec3 ThirdPersonCamera::GetRight() const {
    math::Vec3 forward = GetForward();
    math::Vec3 up(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(forward, up));
}

void ThirdPersonCamera::SetMoveSpeed(float speed) {
    m_moveSpeed = std::max(0.01f, speed);
}

void ThirdPersonCamera::SetSensitivity(float sensitivity) {
    m_sensitivity = std::max(0.01f, sensitivity);
}

void ThirdPersonCamera::SetPitchLimits(float minPitch, float maxPitch) {
    m_minPitch = std::clamp(minPitch, -89.0f, 89.0f);
    m_maxPitch = std::clamp(maxPitch, -89.0f, 89.0f);
    m_pitch = std::clamp(m_pitch, m_minPitch, m_maxPitch);
}

} // namespace test
} // namespace eyelib
