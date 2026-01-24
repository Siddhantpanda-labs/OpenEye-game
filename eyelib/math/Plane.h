#pragma once

#include "eyelib/math/Types.h"
#include <cmath>

namespace eyelib {
namespace math {

struct Plane {
    Vec3 normal{0.0f, 1.0f, 0.0f}; /* normalized */
    float distance = 0.0f;          /* ax+by+cz+d=0 => distance = d */

    Plane() = default;
    Plane(const Vec3& n, float d) : normal(glm::normalize(n)), distance(d) {}

    float Distance(const Vec3& point) const {
        return glm::dot(normal, point) + distance;
    }

    static Plane FromNormalPoint(const Vec3& n, const Vec3& point) {
        Vec3 norm = glm::normalize(n);
        return Plane(norm, -glm::dot(norm, point));
    }

    static Plane FromPoints(const Vec3& p0, const Vec3& p1, const Vec3& p2) {
        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;
        Vec3 n = glm::normalize(glm::cross(e1, e2));
        return FromNormalPoint(n, p0);
    }
};

} // namespace math
} // namespace eyelib
