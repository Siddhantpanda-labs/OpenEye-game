#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Transform.h"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <cmath>
#include <algorithm>

namespace eyelib {
namespace math {

/** Minimum scale component to avoid zero/negative and decomposition issues. */
constexpr float kMinScale = 0.0001f;

/** True if any component is NaN. */
inline bool ContainsNaN(const Vec3& v) {
    return std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z);
}

/** True if any component is NaN. */
inline bool ContainsNaN(const Quat& q) {
    return std::isnan(q.x) || std::isnan(q.y) || std::isnan(q.z) || std::isnan(q.w);
}

/** Replace NaN components with 0. Use for position. */
inline Vec3 SanitizePosition(const Vec3& v) {
    return Vec3(std::isnan(v.x) ? 0.f : v.x, std::isnan(v.y) ? 0.f : v.y, std::isnan(v.z) ? 0.f : v.z);
}

/** Replace NaN with identity quaternion; otherwise normalize. Use for rotation. */
inline Quat SanitizeRotation(const Quat& q) {
    if (ContainsNaN(q)) return Quat(1.f, 0.f, 0.f, 0.f);
    return glm::normalize(q);
}

/** Clamp scale components to [kMinScale, +inf). Ensures scale != 0 for stable TRS. */
inline Vec3 ClampScale(const Vec3& scale) {
    return Vec3(
        std::max(std::abs(scale.x), kMinScale),
        std::max(std::abs(scale.y), kMinScale),
        std::max(std::abs(scale.z), kMinScale)
    );
}

/** Replace NaN components with 1, then ClampScale. Use for scale. */
inline Vec3 SanitizeScale(const Vec3& s) {
    Vec3 r(std::isnan(s.x) ? 1.f : s.x, std::isnan(s.y) ? 1.f : s.y, std::isnan(s.z) ? 1.f : s.z);
    return ClampScale(r);
}

/** Normalize quaternion so rotation is always unit (authoritative data). */
inline Quat NormalizeRotation(const Quat& q) {
    return glm::normalize(q);
}

/** Compose world TRS from parent and local (Unreal-style). No matrices.
 *  world.rotation = parent.rotation * local.rotation
 *  world.scale = parent.scale * local.scale (component-wise)
 *  world.position = parent.rotation * (local.position * parent.scale) + parent.position
 */
inline void ComposeTRS(const Vec3& parentPos, const Quat& parentRot, const Vec3& parentScale,
                      const Vec3& localPos, const Quat& localRot, const Vec3& localScale,
                      Vec3& outWorldPos, Quat& outWorldRot, Vec3& outWorldScale) {
    outWorldRot = parentRot * localRot;
    outWorldRot = glm::normalize(outWorldRot);
    outWorldScale = parentScale * localScale;
    outWorldPos = parentRot * (localPos * parentScale) + parentPos;
}

/** Inverse TRS (for reparent without matrix decompose).
 *  inv.scale = 1/scale, inv.rotation = inverse(rotation),
 *  inv.position = -(inv.rotation * (position * inv.scale))
 */
inline void InverseTRS(const Vec3& position, const Quat& rotation, const Vec3& scale,
                      Vec3& outInvPos, Quat& outInvRot, Vec3& outInvScale) {
    outInvScale = Vec3(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    outInvRot = glm::inverse(rotation);
    outInvPos = -(outInvRot * (position * outInvScale));
}

inline Mat4 Compose(const Vec3& position, const Quat& rotation, const Vec3& scale) {
    const Vec3 p = SanitizePosition(position);
    const Quat q = SanitizeRotation(rotation);
    const Vec3 s = SanitizeScale(scale);
    Mat4 t = glm::translate(Mat4(1.0f), p);
    t = t * glm::mat4_cast(q);
    t = glm::scale(t, s);
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
