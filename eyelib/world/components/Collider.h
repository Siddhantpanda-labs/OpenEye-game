#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace world {
namespace components {

struct Collider {
    enum class Type {
        Box = 0,
        Sphere = 1,
        Capsule = 2
    };

    Type type = Type::Box;
    math::Vec3 size{1.0f, 1.0f, 1.0f};
    float radius = 0.5f;
    float height = 1.0f;
};

} // namespace components
} // namespace world
} // namespace eyelib
