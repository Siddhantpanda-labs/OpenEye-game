#pragma once

#include "api/asset/All.h"
#include "api/world/All.h"

#include <string>
#include <vector>

namespace openeye::player {

struct AssetBindStats {
    int referenced = 0;
    int loaded = 0;
    int missing = 0;
    int unknownType = 0;
};

AssetBindStats BindAssets(const eyelib::api::world::World& world,
                          const std::vector<std::string>& assetRoots,
                          eyelib::api::AssetRegistry& registry,
                          bool allowGpuAssets);

} // namespace openeye::player
