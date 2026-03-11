#include "MaterialLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId MaterialLoader::LoadMaterial(AssetRegistry* registry, const std::string& path) {
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Material);
    return registry->GetId(path);
}

AssetId MaterialLoader::LoadMaterial(const std::string& path) {
    return AssetId::Invalid();
}

bool MaterialLoader::SaveMaterial(const std::string& path, const AssetId& materialAssetId) {
    (void)path;
    (void)materialAssetId;
    return false;
}

} // namespace api
} // namespace eyelib
