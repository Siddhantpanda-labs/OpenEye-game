#pragma once

#include <cstdint>

namespace eyelib {
namespace audio {

/**
 * @brief Audio device management (backend-agnostic).
 */
class AudioDevice {
public:
    AudioDevice() = default;
    ~AudioDevice() = default;

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    bool Initialize();
    void Shutdown();

    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_masterVolume; }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized = false;
    float m_masterVolume = 1.0f;
};

} // namespace audio
} // namespace eyelib
