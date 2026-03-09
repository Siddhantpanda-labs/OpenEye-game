#pragma once

#include "api/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace api {

class AnimationLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadAnimation(const std::string& path);
    static void LoadAnimationAsync(const std::string& path, LoadCallback callback);
};

} // namespace api
} // namespace eyelib
