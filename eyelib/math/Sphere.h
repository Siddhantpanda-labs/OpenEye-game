#pragma once

#include "eyelib/math/Types.h"
#include <cmath>

namespace eyelib {
namespace math {

struct Sphere {
    Vec3 center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;

    Sphere() = default;
    Sphere(const Vec3& center_, float radius_) : center(center_), radius(radius_) {}

    bool Contains(const Vec3& point) const {
        return glm::length(point - center) <= radius;
    }

    bool Intersects(const Sphere& other) const {
        float d = glm::length(center - other.center);
        return d <= (radius + other.radius);
    }

    float Distance(const Vec3& point) const {
        return std::max(0.0f, glm::length(point - center) - radius);
    }

    Sphere Expand(float amount) const {
        return Sphere(center, radius + amount);
    }

    Sphere Union(const Sphere& other) const {
        Vec3 d = other.center - center;
        float dist = glm::length(d);
        if (dist <= 1e-6f) {
            return Sphere(center, glm::max(radius, other.radius));
        }
        float r = (dist + radius + other.radius) * 0.5f;
        Vec3 c = center + d * ((r - radius) / dist);
        return Sphere(c, r);
    }

    /** Approximate AABB transform: scale radius by max scale component. */
    Sphere Transform(const Mat4& matrix) const {
        Vec3 c = Vec3(matrix * Vec4(center, 1.0f));
        Vec3 u(radius, 0, 0), v(0, radius, 0), w(0, 0, radius);
        u = Vec3(matrix * Vec4(u, 0.0f));
        v = Vec3(matrix * Vec4(v, 0.0f));
        w = Vec3(matrix * Vec4(w, 0.0f));
        float r = glm::max(glm::length(u), glm::max(glm::length(v), glm::length(w)));
        return Sphere(c, r);
    }
};

} // namespace math
} // namespace eyelib
