#include "AnimationLoader.h"

#include "eyelib/assets/loaders/AnimationLoader.h"

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

AssetId AnimationLoader::LoadAnimation(const std::string& path) {
    return ToApi(assets::AnimationLoader::LoadAnimation(path));
}

void AnimationLoader::LoadAnimationAsync(const std::string& path, LoadCallback callback) {
    assets::AnimationLoader::LoadAnimationAsync(path, [callback](assets::AssetId id) {
        if (callback) callback(ToApi(id));
    });
}

} // namespace api
} // namespace eyelib
