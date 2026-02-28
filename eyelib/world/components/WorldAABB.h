#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace world {
namespace components {

/** Derived world-space AABB. Written by WorldAABBSystem from Transform + local AABB/Collider. Not serialized. */
struct WorldAABB {
    math::Vec3 min{0.0f, 0.0f, 0.0f};
    math::Vec3 max{0.0f, 0.0f, 0.0f};
};

} // namespace components
} // namespace world
} // namespace eyelib
