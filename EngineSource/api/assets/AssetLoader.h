#pragma once

#include "api/assets/AssetId.h"
#include "api/assets/AssetTypes.h"
#include "api/assets/AssetRegistry.h"

#include <string>

namespace eyelib {
namespace api {

class AssetLoader {
public:
    static AssetId LoadAsset(AssetRegistry& registry, const std::string& path, AssetType type);
    static void UnloadAsset(AssetRegistry& registry, const AssetId& id);
    static bool ReloadAsset(AssetRegistry& registry, const AssetId& id);
};

} // namespace api
} // namespace eyelib
