#include "TextureLoader.h"

#include "eyelib/assets/loaders/TextureLoader.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const assets::AssetId& id) {
    AssetId out;
    out.high = id.High();
    out.low = id.Low();
    return out;
}
} // namespace

AssetId TextureLoader::LoadTexture(const std::string& path, bool isSRGB) {
    return ToApi(assets::TextureLoader::LoadTexture(path, isSRGB));
}

void TextureLoader::LoadTextureAsync(const std::string& path, LoadCallback callback, bool isSRGB) {
    assets::TextureLoader::LoadTextureAsync(path, [callback](assets::AssetId id) {
        if (callback) callback(ToApi(id));
    }, isSRGB);
}

} // namespace api
} // namespace eyelib
