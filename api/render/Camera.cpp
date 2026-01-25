#include "api/render/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace api {
namespace render {

Camera::Camera() = default;

void Camera::SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    m_isPerspective = true;
    m_fov = fovDegrees;
    m_aspect = aspectRatio;
    m_near = nearPlane;
    m_far = farPlane;
    m_projection = glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_isPerspective = false;
    m_near = nearPlane;
    m_far = farPlane;
    m_projection = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
}

void Camera::SetView(const math::Vec3& position, const math::Vec3& target, const math::Vec3& up) {
    m_position = position;
    m_view = glm::lookAt(position, target, up);
}

math::Mat4 Camera::GetViewMatrix() const {
    return m_view;
}

math::Mat4 Camera::GetProjectionMatrix() const {
    return m_projection;
}

} // namespace render
} // namespace api
} // namespace eyelib
