#pragma once

#include "api/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace api {

class TextureLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadTexture(const std::string& path, bool isSRGB = true);
    static void LoadTextureAsync(const std::string& path, LoadCallback callback, bool isSRGB = true);
};

} // namespace api
} // namespace eyelib
