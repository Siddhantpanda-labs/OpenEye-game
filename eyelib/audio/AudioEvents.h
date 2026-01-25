#pragma once

#include "eyelib/audio/AudioClip.h"
#include "eyelib/audio/AudioSource.h"
#include "eyelib/math/Types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace audio {

struct AudioEventParams {
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
};

/**
 * @brief Audio event registry and trigger.
 */
class AudioEvents {
public:
    void RegisterEvent(const std::string& name, const AudioClip& clip, const AudioEventParams& params = {});
    void TriggerEvent(const std::string& name, const math::Vec3& position);

private:
    struct AudioEvent {
        const AudioClip* clip = nullptr;
        AudioEventParams params{};
    };

    void CleanupFinished();

    std::unordered_map<std::string, AudioEvent> m_events;
    std::vector<std::shared_ptr<AudioSource>> m_activeSources;
};

} // namespace audio
} // namespace eyelib
