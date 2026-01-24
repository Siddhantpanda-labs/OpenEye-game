#pragma once

#include <string>

namespace eyelib {
namespace ecs {
namespace components {

struct Name {
    std::string name;
    Name() = default;
    explicit Name(const std::string& n) : name(n) {}
};

} // namespace components
} // namespace ecs
} // namespace eyelib
