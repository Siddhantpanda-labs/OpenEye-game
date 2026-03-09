#pragma once

namespace eyelib {
namespace audio {

struct DistortionSettings {
    float drive = 1.0f;
    float mix = 0.5f;
};

class Distortion {
public:
    void SetSettings(const DistortionSettings& settings) { m_settings = settings; }
    const DistortionSettings& GetSettings() const { return m_settings; }

private:
    DistortionSettings m_settings{};
};

} // namespace audio
} // namespace eyelib
