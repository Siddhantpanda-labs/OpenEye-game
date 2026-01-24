#pragma once

#include "api/ecs/World.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace api {
namespace serialize {

struct Snapshot {
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
};

class WorldSnapshot {
public:
    static Snapshot Capture(const api::World& world);
    static bool Restore(api::World& world, const Snapshot& snapshot);
};

} // namespace serialize
} // namespace api
} // namespace eyelib
