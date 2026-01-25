#include "MaterialLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/loaders/MaterialLoader.h"
#include "eyelib/render/resources/Material.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const assets::AssetId& id) {
    AssetId out;
    out.high = id.High();
    out.low = id.Low();
    return out;
}

assets::AssetId ToCore(const AssetId& id) {
    return assets::AssetId(id.high, id.low);
}
} // namespace

AssetId MaterialLoader::LoadMaterial(const std::string& path) {
    return ToApi(assets::MaterialLoader::LoadMaterial(path));
}

bool MaterialLoader::SaveMaterial(const std::string& path, const AssetId& materialAssetId) {
    auto mat = assets::AssetManager::Get().Cache().Get<eyelib::render::Material>(ToCore(materialAssetId));
    return assets::MaterialLoader::SaveMaterial(path, mat);
}

} // namespace api
} // namespace eyelib
