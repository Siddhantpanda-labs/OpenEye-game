#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace ecs {
namespace components {

struct AABB {
    math::Vec3 min{-0.5f, -0.5f, -0.5f};
    math::Vec3 max{0.5f, 0.5f, 0.5f};
    bool isVisible = true;

    math::Vec3 GetCenter() const { return (min + max) * 0.5f; }
    math::Vec3 GetExtents() const { return (max - min) * 0.5f; }
    math::Vec3 GetSize() const { return max - min; }

    AABB Transform(const math::Mat4& matrix) const;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
