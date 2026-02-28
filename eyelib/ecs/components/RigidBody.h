#pragma once

#include <cstdint>

namespace eyelib {
namespace ecs {
namespace components {

struct RigidBody {
    uint32_t bodyHandle = 0;
    uint32_t shapeHandle = 0;
    bool syncTransform = true;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
