#include "eyelib/audio/AudioEvents.h"

#include <algorithm>

namespace eyelib {
namespace audio {

void AudioEvents::RegisterEvent(const std::string& name, const AudioClip& clip, const AudioEventParams& params) {
    m_events[name] = AudioEvent{ &clip, params };
}

void AudioEvents::TriggerEvent(const std::string& name, const math::Vec3& position) {
    auto it = m_events.find(name);
    if (it == m_events.end() || !it->second.clip) return;

    CleanupFinished();

    auto source = std::make_shared<AudioSource>();
    source->SetPosition(position);
    source->SetVolume(it->second.params.volume);
    source->SetPitch(it->second.params.pitch);
    source->SetLooping(it->second.params.loop);
    source->Play(*it->second.clip);
    m_activeSources.push_back(source);
}

void AudioEvents::CleanupFinished() {
    m_activeSources.erase(
        std::remove_if(m_activeSources.begin(), m_activeSources.end(),
                       [](const std::shared_ptr<AudioSource>& src) { return !src || !src->IsPlaying(); }),
        m_activeSources.end());
}

} // namespace audio
} // namespace eyelib
