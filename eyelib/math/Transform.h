#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace math {

struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z (identity)
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

} // namespace math
} // namespace eyelib

