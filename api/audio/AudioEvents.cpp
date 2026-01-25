#include "api/audio/AudioEvents.h"

#include "api/audio/AudioClip.h"
#include "eyelib/audio/AudioEvents.h"

namespace eyelib {
namespace api {

class AudioEvents::Impl {
public:
    audio::AudioEvents events{};
};

AudioEvents::AudioEvents() : m_impl(new Impl()) {}
AudioEvents::~AudioEvents() {
    delete m_impl;
    m_impl = nullptr;
}

void AudioEvents::RegisterEvent(const std::string& name, const AudioClip& clip, const AudioEventParams& params) {
    audio::AudioEventParams p;
    p.volume = params.volume;
    p.pitch = params.pitch;
    p.loop = params.loop;
    m_impl->events.RegisterEvent(name, *clip.GetInternal(), p);
}

void AudioEvents::TriggerEvent(const std::string& name, const eyelib::math::Vec3& position) {
    m_impl->events.TriggerEvent(name, position);
}

} // namespace api
} // namespace eyelib
