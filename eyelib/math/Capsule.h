#pragma once

#include "eyelib/math/Types.h"
#include <cmath>

namespace eyelib {
namespace math {

struct Capsule {
    Vec3 pointA{0.0f, 0.0f, 0.0f};
    Vec3 pointB{0.0f, 1.0f, 0.0f};
    float radius = 0.5f;

    Capsule() = default;
    Capsule(const Vec3& a, const Vec3& b, float r) : pointA(a), pointB(b), radius(r) {}

    Vec3 GetCenter() const { return (pointA + pointB) * 0.5f; }
    float GetHeight() const { return glm::length(pointB - pointA) + 2.0f * radius; }

    /** Signed distance: negative inside, positive outside. */
    float Distance(const Vec3& point) const {
        Vec3 ab = pointB - pointA;
        float l = glm::length(ab);
        if (l <= 1e-6f) {
            return glm::length(point - pointA) - radius;
        }
        Vec3 ap = point - pointA;
        float t = glm::clamp(glm::dot(ap, ab) / (l * l), 0.0f, 1.0f);
        Vec3 nearest = pointA + t * ab;
        return glm::length(point - nearest) - radius;
    }

    bool Contains(const Vec3& point) const {
        return Distance(point) <= 0.0f;
    }
};

} // namespace math
} // namespace eyelib
