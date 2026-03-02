#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace api {
namespace world { class World; }
namespace serialize {

struct Snapshot {
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
};

class WorldSnapshot {
public:
    static Snapshot Capture(const api::world::World& world);
    static bool Restore(api::world::World& world, const Snapshot& snapshot);
};

} // namespace serialize
} // namespace api
} // namespace eyelib
