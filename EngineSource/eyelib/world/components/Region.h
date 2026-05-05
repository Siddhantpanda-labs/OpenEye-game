#pragma once

#include "eyelib/math/Types.h"

#include <string>
#include <vector>

namespace eyelib {
namespace world {
namespace components {

struct Region {
    enum class Type {
        Box = 0,
        Sphere = 1
    };

    Type type = Type::Box;
    math::Vec3 center{0.0f, 0.0f, 0.0f};
    math::Vec3 size{1.0f, 1.0f, 1.0f};
    float radius = 1.0f;
    int priority = 0;
    bool enabled = true;
    std::vector<std::string> ruleIds;
};

} // namespace components
} // namespace world
} // namespace eyelib
