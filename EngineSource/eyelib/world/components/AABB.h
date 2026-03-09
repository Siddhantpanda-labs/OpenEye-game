#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace world {
namespace components {

struct AABB {
    math::Vec3 min{-0.5f, -0.5f, -0.5f};
    math::Vec3 max{0.5f, 0.5f, 0.5f};
    bool isVisible = true;
};

} // namespace components
} // namespace world
} // namespace eyelib
