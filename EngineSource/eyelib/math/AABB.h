#pragma once

#include "eyelib/math/Types.h"
#include <cfloat>

namespace eyelib {
namespace math {

struct AABB {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};

    AABB() = default;
    AABB(const Vec3& min_, const Vec3& max_) : min(min_), max(max_) {}

    Vec3 GetCenter() const { return (min + max) * 0.5f; }
    Vec3 GetExtents() const { return (max - min) * 0.5f; }
    Vec3 GetSize() const { return max - min; }

    bool IsValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    bool Contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool Intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    AABB Union(const AABB& other) const {
        AABB u;
        u.min = glm::min(min, other.min);
        u.max = glm::max(max, other.max);
        return u;
    }

    void Expand(const Vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void Expand(float amount) {
        Vec3 e(amount, amount, amount);
        min -= e;
        max += e;
    }

    AABB Transform(const Mat4& matrix) const {
        Vec3 corners[8];
        corners[0] = Vec3(min.x, min.y, min.z);
        corners[1] = Vec3(max.x, min.y, min.z);
        corners[2] = Vec3(max.x, max.y, min.z);
        corners[3] = Vec3(min.x, max.y, min.z);
        corners[4] = Vec3(min.x, min.y, max.z);
        corners[5] = Vec3(max.x, min.y, max.z);
        corners[6] = Vec3(max.x, max.y, max.z);
        corners[7] = Vec3(min.x, max.y, max.z);

        Vec3 newMin(FLT_MAX), newMax(-FLT_MAX);
        for (int i = 0; i < 8; ++i) {
            Vec3 t = Vec3(matrix * Vec4(corners[i], 1.0f));
            newMin = glm::min(newMin, t);
            newMax = glm::max(newMax, t);
        }
        return AABB(newMin, newMax);
    }
};

} // namespace math
} // namespace eyelib
