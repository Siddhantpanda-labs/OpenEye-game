#pragma once

#include <cstdint>

namespace eyelib {
namespace world {
namespace components {

struct RigidBody {
    uint32_t bodyHandle = 0;
    uint32_t shapeHandle = 0;
    float mass = 1.0f;
    bool useGravity = true;
    bool isKinematic = false;
    bool syncTransform = true;
};

} // namespace components
} // namespace world
} // namespace eyelib
