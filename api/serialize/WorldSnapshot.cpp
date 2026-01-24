#include "WorldSnapshot.h"

#include "eyelib/serialize/WorldSnapshot.h"

namespace eyelib {
namespace api {
namespace serialize {

Snapshot WorldSnapshot::Capture(const api::World& world) {
    const auto snap = eyelib::serialize::WorldSnapshot::Capture(*world.GetInternal());
    Snapshot out;
    out.data = snap.data;
    out.timestamp = snap.timestamp;
    return out;
}

bool WorldSnapshot::Restore(api::World& world, const Snapshot& snapshot) {
    eyelib::serialize::Snapshot snap;
    snap.data = snapshot.data;
    snap.timestamp = snapshot.timestamp;
    return eyelib::serialize::WorldSnapshot::Restore(*world.GetInternal(), snap);
}

} // namespace serialize
} // namespace api
} // namespace eyelib
