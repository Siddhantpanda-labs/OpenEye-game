#include "api/audio/AudioDevice.h"

#include "eyelib/audio/AudioDevice.h"

namespace eyelib {
namespace api {

class AudioDevice::Impl {
public:
    audio::AudioDevice device{};
};

AudioDevice::AudioDevice() : m_impl(new Impl()) {}
AudioDevice::~AudioDevice() {
    delete m_impl;
    m_impl = nullptr;
}

bool AudioDevice::Initialize() { return m_impl->device.Initialize(); }
void AudioDevice::Shutdown() { m_impl->device.Shutdown(); }
void AudioDevice::SetMasterVolume(float volume) { m_impl->device.SetMasterVolume(volume); }
float AudioDevice::GetMasterVolume() const { return m_impl->device.GetMasterVolume(); }

} // namespace api
} // namespace eyelib
