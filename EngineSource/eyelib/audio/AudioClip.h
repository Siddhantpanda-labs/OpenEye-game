#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace audio {

/**
 * @brief Audio clip data (PCM float samples).
 */
class AudioClip {
public:
    /** Load from file (WAV supported; reads file and parses via LoadFromMemory). */
    bool LoadFromFile(const std::string& path);
    /** Load from in-memory WAV data. */
    bool LoadFromMemory(const uint8_t* data, size_t sizeBytes);

    float GetDuration() const;
    int GetSampleRate() const { return m_sampleRate; }
    int GetChannels() const { return m_channels; }
    const std::vector<float>& GetSamples() const { return m_samples; }

private:
    int m_sampleRate = 0;
    int m_channels = 0;
    std::vector<float> m_samples;
};

} // namespace audio
} // namespace eyelib
