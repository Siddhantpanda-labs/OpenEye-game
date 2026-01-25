#pragma once

#include "api/audio/AudioEffects.h"
#include "eyelib/math/Types.h"
#include <string>

namespace eyelib {
namespace audio { class AudioSource; }
namespace api {

class AudioClip;

class AudioSource {
public:
    AudioSource();
    ~AudioSource();

    AudioSource(const AudioSource&) = delete;
    AudioSource& operator=(const AudioSource&) = delete;

    void Play(const AudioClip& clip);
    void Stop();
    void Pause();

    void SetVolume(float volume);
    void SetPitch(float pitch);
    void SetLooping(bool loop);
    void SetPosition(const eyelib::math::Vec3& position);
    void SetVelocity(const eyelib::math::Vec3& velocity);

    bool IsPlaying() const;
    float GetTime() const;

    bool PlayStream(const std::string& path, bool loop);
    void UpdateStream();
    bool IsStreaming() const;

    void ApplyReverb(const ReverbSettings& settings);
    void ApplyEcho(const EchoSettings& settings);
    void ApplyFilter(const FilterSettings& settings);

private:
    class Impl;
    Impl* m_impl = nullptr;

    audio::AudioSource* GetInternal();
    friend class AudioMixer;
};

} // namespace api
} // namespace eyelib
