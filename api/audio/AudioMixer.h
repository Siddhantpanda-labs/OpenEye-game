#pragma once

#include <cstdint>
#include <string>

namespace eyelib {
namespace audio { class AudioMixer; }
namespace api {

class AudioSource;

using AudioGroupHandle = uint32_t;

class AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    AudioGroupHandle CreateGroup(const std::string& name);
    void SetGroupVolume(AudioGroupHandle group, float volume);
    void AddSourceToGroup(AudioSource* source, AudioGroupHandle group);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib
