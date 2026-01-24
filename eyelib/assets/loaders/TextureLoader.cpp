#include "TextureLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/render/resources/TextureLoader.h"

#include <thread>

namespace eyelib {
namespace assets {

AssetId TextureLoader::LoadTexture(const std::string& path, bool isSRGB) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Texture");
    std::shared_ptr<render::Texture2D> tex;
    if (path.rfind(".hdr") != std::string::npos || path.rfind(".HDR") != std::string::npos) {
        tex = render::TextureLoader::LoadHDR(path);
    } else if (path.rfind(".png") != std::string::npos || path.rfind(".PNG") != std::string::npos) {
        tex = render::TextureLoader::LoadPNG(path, isSRGB, true);
    } else {
        tex = render::TextureLoader::LoadJPEG(path, isSRGB, true);
    }
    if (tex) {
        tex->SetAssetId(id);
        manager.Cache().Store(id, tex, "Texture");
    }
    return id;
}

void TextureLoader::LoadTextureAsync(const std::string& path, LoadCallback callback, bool isSRGB) {
    std::thread([path, callback, isSRGB]() {
        const AssetId id = LoadTexture(path, isSRGB);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib
