#include "AudioLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId AudioLoader::LoadAudio(AssetRegistry* registry, const std::string& path) {
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Audio);
    return registry->GetId(path);
}

AssetId AudioLoader::LoadAudio(const std::string& path) {
    (void)path;
    return AssetId::Invalid();
}

void AudioLoader::LoadAudioAsync(const std::string& path, LoadCallback callback) {
    if (callback) callback(AssetId::Invalid());
}

} // namespace api
} // namespace eyelib
