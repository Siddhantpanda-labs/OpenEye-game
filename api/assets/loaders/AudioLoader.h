#pragma once

#include "api/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace api {

class AudioLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadAudio(const std::string& path);
    static void LoadAudioAsync(const std::string& path, LoadCallback callback);
};

} // namespace api
} // namespace eyelib
