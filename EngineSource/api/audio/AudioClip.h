#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace eyelib {
namespace audio { class AudioClip; }
namespace api {

class AudioClip {
public:
    AudioClip();
    ~AudioClip();

    AudioClip(const AudioClip&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;

    bool LoadFromFile(const std::string& path);
    bool LoadFromMemory(const uint8_t* data, size_t sizeBytes);

    float GetDuration() const;
    int GetSampleRate() const;
    int GetChannels() const;

private:
    class Impl;
    Impl* m_impl = nullptr;

    const audio::AudioClip* GetInternal() const;
    friend class AudioSource;
    friend class AudioEvents;
};

} // namespace api
} // namespace eyelib
