#pragma once

#include "eyelib/world/EntityId.h"

namespace eyelib {
namespace world {
namespace components {

struct RegionMembership {
    EntityId region = InvalidEntity;
};

} // namespace components
} // namespace world
} // namespace eyelib
