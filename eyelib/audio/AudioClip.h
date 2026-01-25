#pragma once

#include "eyelib/assets/AssetId.h"

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
    bool LoadFromFile(const std::string& path);
    bool LoadFromMemory(const uint8_t* data, size_t sizeBytes);

    float GetDuration() const;
    int GetSampleRate() const { return m_sampleRate; }
    int GetChannels() const { return m_channels; }
    const std::vector<float>& GetSamples() const { return m_samples; }

private:
    assets::AssetId m_assetId{};
    int m_sampleRate = 0;
    int m_channels = 0;
    std::vector<float> m_samples;
};

} // namespace audio
} // namespace eyelib
