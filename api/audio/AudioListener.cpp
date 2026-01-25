#include "api/audio/AudioListener.h"

#include "eyelib/audio/AudioListener.h"

namespace eyelib {
namespace api {

class AudioListener::Impl {
public:
    audio::AudioListener listener{};
};

AudioListener::AudioListener() : m_impl(new Impl()) {}
AudioListener::~AudioListener() {
    delete m_impl;
    m_impl = nullptr;
}

void AudioListener::SetPosition(const eyelib::math::Vec3& position) { m_impl->listener.SetPosition(position); }
void AudioListener::SetOrientation(const eyelib::math::Vec3& forward, const eyelib::math::Vec3& up) {
    m_impl->listener.SetOrientation(forward, up);
}
void AudioListener::SetVelocity(const eyelib::math::Vec3& velocity) { m_impl->listener.SetVelocity(velocity); }

} // namespace api
} // namespace eyelib
