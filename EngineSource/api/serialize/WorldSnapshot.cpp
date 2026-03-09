#include "WorldSnapshot.h"

#include "api/world/World.h"
#include "eyelib/world/Serialization/WorldSnapshot.h"
#include "eyelib/world/Serialization/WorldSerializer.h"

#include <chrono>
#include <cstring>
#include <json.hpp>

namespace eyelib {
namespace api {
namespace serialize {

Snapshot WorldSnapshot::Capture(const api::world::World& world) {
    Snapshot out;
    out.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    ::eyelib::world::WorldSerializer serializer;
    const auto& state = world.GetInternal()->GetState();
    auto snap = ::eyelib::world::WorldSnapshot::Capture(state, serializer);
    std::string jsonStr = snap.GetData().dump();
    out.data.assign(jsonStr.begin(), jsonStr.end());
    return out;
}

bool WorldSnapshot::Restore(api::world::World& world, const Snapshot& snapshot) {
    if (snapshot.data.empty()) return false;
    try {
        std::string jsonStr(snapshot.data.begin(), snapshot.data.end());
        auto data = nlohmann::json::parse(jsonStr);
        ::eyelib::world::WorldSnapshot snap(data);
        ::eyelib::world::WorldSerializer serializer;
        auto& state = world.GetInternal()->GetState();
        return snap.Restore(state, serializer);
    } catch (...) {
        return false;
    }
}

} // namespace serialize
} // namespace api
} // namespace eyelib
