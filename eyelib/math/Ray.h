#pragma once

#include "eyelib/math/Types.h"
#include <cmath>

namespace eyelib {
namespace math {

struct Ray {
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f}; /* normalized */

    Ray() = default;
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(glm::normalize(d)) {}

    Vec3 GetPoint(float t) const { return origin + direction * t; }
    float Distance(const Vec3& point) const {
        Vec3 ap = point - origin;
        float t = glm::dot(ap, direction);
        Vec3 closest = (t >= 0.0f) ? GetPoint(t) : origin;
        return glm::length(point - closest);
    }
    Vec3 ClosestPoint(const Vec3& point) const {
        float t = glm::dot(point - origin, direction);
        return GetPoint(glm::max(0.0f, t));
    }
};

} // namespace math
} // namespace eyelib
