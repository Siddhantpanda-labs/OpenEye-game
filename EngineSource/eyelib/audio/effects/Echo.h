#pragma once

namespace eyelib {
namespace audio {

struct EchoSettings {
    float delay = 0.25f;
    float feedback = 0.3f;
    float wetLevel = 0.4f;
};

class Echo {
public:
    void SetSettings(const EchoSettings& settings) { m_settings = settings; }
    const EchoSettings& GetSettings() const { return m_settings; }

private:
    EchoSettings m_settings{};
};

} // namespace audio
} // namespace eyelib
