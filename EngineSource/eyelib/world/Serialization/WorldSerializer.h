#pragma once

#include "eyelib/world/WorldState.h"

#include <json.hpp>
#include <string>

namespace eyelib {
namespace world {

class WorldSerializer {
public:
    bool Save(const WorldState& state, const std::string& path) const;
    bool Load(WorldState& state, const std::string& path) const;
    nlohmann::json Serialize(const WorldState& state) const;
    bool Deserialize(WorldState& state, const nlohmann::json& data) const;

private:
    static constexpr uint32_t kVersion = 1;
};

} // namespace world
} // namespace eyelib
