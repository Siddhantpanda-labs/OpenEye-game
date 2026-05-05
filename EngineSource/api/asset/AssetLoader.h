#pragma once

#include "api/asset/AssetId.h"
#include "api/asset/AssetTypes.h"
#include "api/asset/AssetRegistry.h"

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
