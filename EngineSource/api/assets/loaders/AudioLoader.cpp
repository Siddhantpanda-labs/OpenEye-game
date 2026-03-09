#include "AudioLoader.h"

#include "eyelib/assets/loaders/AudioLoader.h"

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

AssetId AudioLoader::LoadAudio(const std::string& path) {
    return ToApi(assets::AudioLoader::LoadAudio(path));
}

void AudioLoader::LoadAudioAsync(const std::string& path, LoadCallback callback) {
    assets::AudioLoader::LoadAudioAsync(path, [callback](assets::AssetId id) {
        if (callback) callback(ToApi(id));
    });
}

} // namespace api
} // namespace eyelib
