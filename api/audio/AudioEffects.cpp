#include "api/audio/AudioEffects.h"
#include "eyelib/audio/effects/Reverb.h"
#include "eyelib/audio/effects/Echo.h"
#include "eyelib/audio/effects/Filter.h"
#include "eyelib/audio/effects/Distortion.h"

namespace eyelib {
namespace api {

class Reverb::Impl {
public:
    eyelib::audio::Reverb effect;
};

Reverb::Reverb() : m_impl(new Impl()) {}
Reverb::~Reverb() { delete m_impl; m_impl = nullptr; }

void Reverb::SetSettings(const ReverbSettings& settings) {
    eyelib::audio::ReverbSettings internal;
    internal.roomSize = settings.roomSize;
    internal.damping = settings.damping;
    internal.wetLevel = settings.wetLevel;
    internal.dryLevel = settings.dryLevel;
    m_impl->effect.SetSettings(internal);
}

ReverbSettings Reverb::GetSettings() const {
    ReverbSettings cached{};
    const auto& internal = m_impl->effect.GetSettings();
    cached.roomSize = internal.roomSize;
    cached.damping = internal.damping;
    cached.wetLevel = internal.wetLevel;
    cached.dryLevel = internal.dryLevel;
    return cached;
}

class Echo::Impl {
public:
    eyelib::audio::Echo effect;
};

Echo::Echo() : m_impl(new Impl()) {}
Echo::~Echo() { delete m_impl; m_impl = nullptr; }

void Echo::SetSettings(const EchoSettings& settings) {
    eyelib::audio::EchoSettings internal;
    internal.delay = settings.delay;
    internal.feedback = settings.feedback;
    internal.wetLevel = settings.wetLevel;
    m_impl->effect.SetSettings(internal);
}

EchoSettings Echo::GetSettings() const {
    EchoSettings cached{};
    const auto& internal = m_impl->effect.GetSettings();
    cached.delay = internal.delay;
    cached.feedback = internal.feedback;
    cached.wetLevel = internal.wetLevel;
    return cached;
}

class Filter::Impl {
public:
    eyelib::audio::Filter effect;
};

Filter::Filter() : m_impl(new Impl()) {}
Filter::~Filter() { delete m_impl; m_impl = nullptr; }

void Filter::SetSettings(const FilterSettings& settings) {
    eyelib::audio::FilterSettings internal;
    internal.type = static_cast<eyelib::audio::FilterType>(settings.type);
    internal.cutoff = settings.cutoff;
    internal.resonance = settings.resonance;
    m_impl->effect.SetSettings(internal);
}

FilterSettings Filter::GetSettings() const {
    FilterSettings cached{};
    const auto& internal = m_impl->effect.GetSettings();
    cached.type = static_cast<FilterType>(internal.type);
    cached.cutoff = internal.cutoff;
    cached.resonance = internal.resonance;
    return cached;
}

class Distortion::Impl {
public:
    eyelib::audio::Distortion effect;
};

Distortion::Distortion() : m_impl(new Impl()) {}
Distortion::~Distortion() { delete m_impl; m_impl = nullptr; }

void Distortion::SetSettings(const DistortionSettings& settings) {
    eyelib::audio::DistortionSettings internal;
    internal.drive = settings.drive;
    internal.mix = settings.mix;
    m_impl->effect.SetSettings(internal);
}

DistortionSettings Distortion::GetSettings() const {
    DistortionSettings cached{};
    const auto& internal = m_impl->effect.GetSettings();
    cached.drive = internal.drive;
    cached.mix = internal.mix;
    return cached;
}

} // namespace api
} // namespace eyelib
