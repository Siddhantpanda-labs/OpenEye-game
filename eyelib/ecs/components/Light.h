#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace ecs {
namespace components {

struct Light {
    enum class Type {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    Type type = Type::Directional;
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float innerConeAngle = 15.0f;
    float outerConeAngle = 30.0f;
    bool castShadows = true;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
