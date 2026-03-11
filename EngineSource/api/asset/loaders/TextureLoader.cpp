#include "TextureLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId TextureLoader::LoadTexture(AssetRegistry* registry, const std::string& path, bool isSRGB) {
    (void)isSRGB;
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Texture);
    return registry->GetId(path);
}

AssetId TextureLoader::LoadTexture(const std::string& path, bool isSRGB) {
    (void)path;
    (void)isSRGB;
    return AssetId::Invalid();
}

void TextureLoader::LoadTextureAsync(const std::string& path, LoadCallback callback, bool isSRGB) {
    (void)path;
    (void)isSRGB;
    if (callback) callback(AssetId::Invalid());
}

} // namespace api
} // namespace eyelib
