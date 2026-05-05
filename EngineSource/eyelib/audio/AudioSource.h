#pragma once

#include "eyelib/audio/AudioClip.h"
#include "eyelib/audio/effects/Echo.h"
#include "eyelib/audio/effects/Filter.h"
#include "eyelib/audio/effects/Reverb.h"
#include "eyelib/math/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace eyelib {
namespace audio {

/**
 * @brief Audio playback source (backend-agnostic).
 */
class AudioSource {
public:
    struct StreamState {
        std::string path;
        uint32_t dataOffset = 0;
        uint32_t dataSize = 0;
        uint32_t cursor = 0;
        int sampleRate = 0;
        int channels = 0;
        uint16_t bitsPerSample = 0;
        bool loop = false;
        bool isOgg = false;
        void* oggHandle = nullptr;
        bool isMp3 = false;
        void* mfReader = nullptr;
        bool mfInit = false;
        unsigned int buffers[4]{};
        size_t bufferCount = 0;
    };

    AudioSource();
    ~AudioSource();

    void Play(const AudioClip& clip);
    void Stop();
    void Pause();

    void SetVolume(float volume);
    void SetPitch(float pitch);
    void SetLooping(bool loop);
    void SetPosition(const math::Vec3& position);
    void SetVelocity(const math::Vec3& velocity);
    void SetGroupVolume(float volume);

    bool IsPlaying() const;
    float GetTime() const;

    bool PlayStream(const std::string& path, bool loop);
    void UpdateStream();
    bool IsStreaming() const { return m_streaming; }

    void ApplyReverb(const ReverbSettings& settings);
    void ApplyEcho(const EchoSettings& settings);
    void ApplyFilter(const FilterSettings& settings);

    float GetVolume() const { return m_volume; }
    float GetPitch() const { return m_pitch; }
    bool IsLooping() const { return m_looping; }
    math::Vec3 GetPosition() const { return m_position; }
    math::Vec3 GetVelocity() const { return m_velocity; }

private:
    const AudioClip* m_clip = nullptr;
    bool m_playing = false;
    bool m_paused = false;
    bool m_looping = false;
    float m_volume = 1.0f;
    float m_groupVolume = 1.0f;
    float m_pitch = 1.0f;

    float m_startTime = 0.0f;
    float m_offset = 0.0f;

    math::Vec3 m_position{0.0f};
    math::Vec3 m_velocity{0.0f};

    unsigned int m_alSource = 0;
    unsigned int m_alBuffer = 0;

    bool m_streaming = false;
    StreamState m_stream{};

    unsigned int m_reverbEffect = 0;
    unsigned int m_reverbSlot = 0;
    unsigned int m_echoEffect = 0;
    unsigned int m_echoSlot = 0;
    unsigned int m_filter = 0;
};

} // namespace audio
} // namespace eyelib
