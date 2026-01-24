#pragma once

#include "eyelib/ecs/World.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace serialize {

struct Snapshot {
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
};

class WorldSnapshot {
public:
    static Snapshot Capture(const ecs::World& world);
    static bool Restore(ecs::World& world, const Snapshot& snapshot);
};

} // namespace serialize
} // namespace eyelib
