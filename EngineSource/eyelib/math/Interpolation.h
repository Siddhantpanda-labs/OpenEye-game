#pragma once

#include "eyelib/math/Types.h"
#include <cmath>

namespace eyelib {
namespace math {

template <typename T>
inline T Lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

inline Quat Slerp(const Quat& a, const Quat& b, float t) {
    return glm::slerp(a, b, t);
}

inline Quat NLerp(const Quat& a, const Quat& b, float t) {
    return glm::normalize(Lerp(a, b, t));
}

inline Vec3 Bezier(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
    float u = 1.0f - t;
    float u2 = u * u;
    float u3 = u2 * u;
    float t2 = t * t;
    float t3 = t2 * t;
    return u3 * p0 + 3.0f * u2 * t * p1 + 3.0f * u * t2 * p2 + t3 * p3;
}

inline Vec3 CatmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

inline float EaseIn(float t, float power = 2.0f) {
    return std::pow(t, power);
}

inline float EaseOut(float t, float power = 2.0f) {
    return 1.0f - std::pow(1.0f - t, power);
}

inline float EaseInOut(float t, float power = 2.0f) {
    return t < 0.5f
        ? 0.5f * std::pow(2.0f * t, power)
        : 1.0f - 0.5f * std::pow(2.0f * (1.0f - t), power);
}

} // namespace math
} // namespace eyelib
