#pragma once

#include "api/asset/AssetId.h"

#include <functional>
#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace eyelib {
namespace api {

class AnimationLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadAnimation(AssetRegistry* registry, const std::string& path);
    static AssetId LoadAnimation(const std::string& path);
    static void LoadAnimationAsync(const std::string& path, LoadCallback callback);
};

} // namespace api
} // namespace eyelib
