#include "api/audio/AudioMixer.h"

#include "api/audio/AudioSource.h"
#include "eyelib/audio/AudioMixer.h"

namespace eyelib {
namespace api {

class AudioMixer::Impl {
public:
    audio::AudioMixer mixer{};
};

AudioMixer::AudioMixer() : m_impl(new Impl()) {}
AudioMixer::~AudioMixer() {
    delete m_impl;
    m_impl = nullptr;
}

AudioGroupHandle AudioMixer::CreateGroup(const std::string& name) {
    return m_impl->mixer.CreateGroup(name);
}

void AudioMixer::SetGroupVolume(AudioGroupHandle group, float volume) {
    m_impl->mixer.SetGroupVolume(group, volume);
}

void AudioMixer::AddSourceToGroup(AudioSource* source, AudioGroupHandle group) {
    if (!source) return;
    m_impl->mixer.AddSourceToGroup(source->GetInternal(), group);
}

} // namespace api
} // namespace eyelib
