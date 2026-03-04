#pragma once

#include "eyelib/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace assets {

class TextureLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadTexture(const std::string& path, bool isSRGB = true);
    static void LoadTextureAsync(const std::string& path, LoadCallback callback, bool isSRGB = true);

    /** CPU-only: store TextureDescriptor (path + options); no pixel load or GPU. For binding phase. */
    static AssetId LoadTextureDescriptorOnly(const std::string& path, bool isSRGB = true);
};

} // namespace assets
} // namespace eyelib
