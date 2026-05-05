#pragma once

#include "eyelib/world/WorldState.h"

#include <json.hpp>

namespace eyelib {
namespace world {

class WorldSerializer;

class WorldSnapshot {
public:
    WorldSnapshot() = default;
    explicit WorldSnapshot(nlohmann::json data);

    static WorldSnapshot Capture(const WorldState& state, const WorldSerializer& serializer);
    bool Restore(WorldState& state, const WorldSerializer& serializer) const;

    const nlohmann::json& GetData() const;

private:
    nlohmann::json m_data;
};

} // namespace world
} // namespace eyelib
