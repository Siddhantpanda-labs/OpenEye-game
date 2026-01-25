#pragma once

namespace eyelib {
namespace api {

struct ReverbSettings {
    float roomSize = 0.5f;
    float damping = 0.5f;
    float wetLevel = 0.3f;
    float dryLevel = 0.7f;
};

struct EchoSettings {
    float delay = 0.25f;
    float feedback = 0.3f;
    float wetLevel = 0.4f;
};

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

} // namespace api
} // namespace eyelib
