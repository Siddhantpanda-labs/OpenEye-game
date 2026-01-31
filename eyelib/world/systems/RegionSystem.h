#pragma once

#include "eyelib/world/WorldState.h"

namespace eyelib {
namespace world {
namespace systems {

class RegionSystem {
public:
    void Update(WorldState& state) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
