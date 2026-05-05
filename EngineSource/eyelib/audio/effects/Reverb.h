#pragma once

namespace eyelib {
namespace audio {

struct ReverbSettings {
    float roomSize = 0.5f;
    float damping = 0.5f;
    float wetLevel = 0.3f;
    float dryLevel = 0.7f;
};

class Reverb {
public:
    void SetSettings(const ReverbSettings& settings) { m_settings = settings; }
    const ReverbSettings& GetSettings() const { return m_settings; }

private:
    ReverbSettings m_settings{};
};

} // namespace audio
} // namespace eyelib
