#pragma once

#include "eyelib/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace assets {

class AudioLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadAudio(const std::string& path);
    static void LoadAudioAsync(const std::string& path, LoadCallback callback);
};

} // namespace assets
} // namespace eyelib
