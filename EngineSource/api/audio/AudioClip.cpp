#include "api/audio/AudioClip.h"

#include "eyelib/audio/AudioClip.h"

namespace eyelib {
namespace api {

class AudioClip::Impl {
public:
    audio::AudioClip clip{};
};

AudioClip::AudioClip() : m_impl(new Impl()) {}
AudioClip::~AudioClip() {
    delete m_impl;
    m_impl = nullptr;
}

bool AudioClip::LoadFromFile(const std::string& path) {
    return m_impl->clip.LoadFromFile(path);
}

bool AudioClip::LoadFromMemory(const uint8_t* data, size_t sizeBytes) {
    return m_impl->clip.LoadFromMemory(data, sizeBytes);
}

float AudioClip::GetDuration() const { return m_impl->clip.GetDuration(); }
int AudioClip::GetSampleRate() const { return m_impl->clip.GetSampleRate(); }
int AudioClip::GetChannels() const { return m_impl->clip.GetChannels(); }

const audio::AudioClip* AudioClip::GetInternal() const { return &m_impl->clip; }

} // namespace api
} // namespace eyelib
