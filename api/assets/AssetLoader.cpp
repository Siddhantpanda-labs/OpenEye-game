#include "AssetLoader.h"

namespace eyelib {
namespace api {

AssetId AssetLoader::LoadAsset(AssetRegistry& registry, const std::string& path, AssetType type) {
    return registry.LoadAsset(path, type);
}

void AssetLoader::UnloadAsset(AssetRegistry& registry, const AssetId& id) {
    registry.UnloadAsset(id);
}

bool AssetLoader::ReloadAsset(AssetRegistry& registry, const AssetId& id) {
    return registry.ReloadAsset(id);
}

} // namespace api
} // namespace eyelib
