#include "AnimationLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId AnimationLoader::LoadAnimation(AssetRegistry* registry, const std::string& path) {
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Animation);
    return registry->GetId(path);
}

AssetId AnimationLoader::LoadAnimation(const std::string& path) {
    (void)path;
    return AssetId::Invalid();
}

void AnimationLoader::LoadAnimationAsync(const std::string& path, LoadCallback callback) {
    if (callback) callback(AssetId::Invalid());
}

} // namespace api
} // namespace eyelib
