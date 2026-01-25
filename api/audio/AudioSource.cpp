#include "api/audio/AudioSource.h"

#include "api/audio/AudioClip.h"
#include "eyelib/audio/AudioSource.h"

namespace eyelib {
namespace api {

class AudioSource::Impl {
public:
    audio::AudioSource source{};
};

AudioSource::AudioSource() : m_impl(new Impl()) {}
AudioSource::~AudioSource() {
    delete m_impl;
    m_impl = nullptr;
}

void AudioSource::Play(const AudioClip& clip) {
    auto internal = clip.GetInternal();
    if (!internal) return;
    m_impl->source.Play(*internal);
}

void AudioSource::Stop() { m_impl->source.Stop(); }
void AudioSource::Pause() { m_impl->source.Pause(); }

void AudioSource::SetVolume(float volume) { m_impl->source.SetVolume(volume); }
void AudioSource::SetPitch(float pitch) { m_impl->source.SetPitch(pitch); }
void AudioSource::SetLooping(bool loop) { m_impl->source.SetLooping(loop); }
void AudioSource::SetPosition(const eyelib::math::Vec3& position) { m_impl->source.SetPosition(position); }
void AudioSource::SetVelocity(const eyelib::math::Vec3& velocity) { m_impl->source.SetVelocity(velocity); }

bool AudioSource::IsPlaying() const { return m_impl->source.IsPlaying(); }
float AudioSource::GetTime() const { return m_impl->source.GetTime(); }

bool AudioSource::PlayStream(const std::string& path, bool loop) { return m_impl->source.PlayStream(path, loop); }
void AudioSource::UpdateStream() { m_impl->source.UpdateStream(); }
bool AudioSource::IsStreaming() const { return m_impl->source.IsStreaming(); }

void AudioSource::ApplyReverb(const ReverbSettings& settings) {
    audio::ReverbSettings s;
    s.roomSize = settings.roomSize;
    s.damping = settings.damping;
    s.wetLevel = settings.wetLevel;
    s.dryLevel = settings.dryLevel;
    m_impl->source.ApplyReverb(s);
}

void AudioSource::ApplyEcho(const EchoSettings& settings) {
    audio::EchoSettings s;
    s.delay = settings.delay;
    s.feedback = settings.feedback;
    s.wetLevel = settings.wetLevel;
    m_impl->source.ApplyEcho(s);
}

void AudioSource::ApplyFilter(const FilterSettings& settings) {
    audio::FilterSettings s;
    switch (settings.type) {
        case FilterType::HighPass: s.type = audio::FilterType::HighPass; break;
        case FilterType::BandPass: s.type = audio::FilterType::BandPass; break;
        default: s.type = audio::FilterType::LowPass; break;
    }
    s.cutoff = settings.cutoff;
    s.resonance = settings.resonance;
    m_impl->source.ApplyFilter(s);
}

audio::AudioSource* AudioSource::GetInternal() { return &m_impl->source; }

} // namespace api
} // namespace eyelib
