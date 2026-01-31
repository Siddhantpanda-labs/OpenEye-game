#pragma once

#include <string>
#include <vector>

namespace eyelib {
namespace world {
namespace components {

struct ActiveSystems {
    std::vector<std::string> enabledSystems;
};

} // namespace components
} // namespace world
} // namespace eyelib
