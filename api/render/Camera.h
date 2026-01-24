#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>

namespace eyelib {
namespace api {

/**
 * @brief Camera API. View/projection matrices, position, FOV, etc.
 */
class Camera {
public:
    Camera();
    ~Camera() = default;

    /** Set perspective projection. */
    void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);

    /** Set orthographic projection. */
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

    /** Set view matrix (look-at). */
    void SetView(const math::Vec3& position, const math::Vec3& target, const math::Vec3& up);

    /** Get view matrix. */
    math::Mat4 GetViewMatrix() const;

    /** Get projection matrix. */
    math::Mat4 GetProjectionMatrix() const;

    /** Get view position. */
    math::Vec3 GetPosition() const { return m_position; }
    void SetPosition(const math::Vec3& pos) { m_position = pos; }

private:
    math::Vec3 m_position{0, 0, 0};
    math::Mat4 m_view{1.0f};
    math::Mat4 m_projection{1.0f};
    bool m_isPerspective = true;
    float m_fov = 60.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
};

} // namespace api
} // namespace eyelib
