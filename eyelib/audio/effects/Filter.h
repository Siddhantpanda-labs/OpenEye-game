#pragma once

namespace eyelib {
namespace audio {

enum class FilterType {
    LowPass,
    HighPass,
    BandPass
};

struct FilterSettings {
    FilterType type = FilterType::LowPass;
    float cutoff = 8000.0f;
    float resonance = 0.7f;
};

class Filter {
public:
    void SetSettings(const FilterSettings& settings) { m_settings = settings; }
    const FilterSettings& GetSettings() const { return m_settings; }

private:
    FilterSettings m_settings{};
};

} // namespace audio
} // namespace eyelib
