#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Transform.h"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <cmath>

namespace eyelib {
namespace math {

inline Mat4 Compose(const Vec3& position, const Quat& rotation, const Vec3& scale) {
    Mat4 t = glm::translate(Mat4(1.0f), position);
    t = t * glm::mat4_cast(rotation);
    t = glm::scale(t, scale);
    return t;
}

inline void Decompose(const Mat4& m, Vec3& position, Quat& rotation, Vec3& scale) {
    Vec3 skew;
    Vec4 persp;
    glm::decompose(m, scale, rotation, position, skew, persp);
}

inline Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    return glm::lookAt(eye, target, up);
}

inline Mat4 Perspective(float fovYRadians, float aspect, float nearZ, float farZ) {
    return glm::perspective(fovYRadians, aspect, nearZ, farZ);
}

inline Mat4 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ) {
    return glm::ortho(left, right, bottom, top, nearZ, farZ);
}

inline Mat4 Inverse(const Mat4& m) {
    return glm::inverse(m);
}

inline Mat4 Transpose(const Mat4& m) {
    return glm::transpose(m);
}

/** Upper-left 3x3 inverse transpose for normal transformation. */
inline Mat3 NormalMatrix(const Mat4& m) {
    return glm::mat3(glm::transpose(glm::inverse(m)));
}

/** Mat4 with upper-left 3x3 from m and (0,0,0,1) as fourth column (for passing Mat3 as uniform). */
inline Mat4 Mat4FromMat3(const Mat3& m) {
    return Mat4(Vec4(m[0], 0.0f), Vec4(m[1], 0.0f), Vec4(m[2], 0.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

} // namespace math
} // namespace eyelib
