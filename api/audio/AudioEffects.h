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

struct DistortionSettings {
    float drive = 1.0f;
    float mix = 0.5f;
};

class Reverb {
public:
    Reverb();
    ~Reverb();
    void SetSettings(const ReverbSettings& settings);
    ReverbSettings GetSettings() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

class Echo {
public:
    Echo();
    ~Echo();
    void SetSettings(const EchoSettings& settings);
    EchoSettings GetSettings() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

class Filter {
public:
    Filter();
    ~Filter();
    void SetSettings(const FilterSettings& settings);
    FilterSettings GetSettings() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

class Distortion {
public:
    Distortion();
    ~Distortion();
    void SetSettings(const DistortionSettings& settings);
    DistortionSettings GetSettings() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib
