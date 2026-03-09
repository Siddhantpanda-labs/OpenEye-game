#include "eyelib/audio/AudioListener.h"

#include <AL/al.h>

namespace eyelib {
namespace audio {

// Keep listener state synced to OpenAL.

AudioListener::AudioListener() = default;

void AudioListener::SetPosition(const math::Vec3& position) {
    m_position = position;
    alListener3f(AL_POSITION, m_position.x, m_position.y, m_position.z);
}

void AudioListener::SetOrientation(const math::Vec3& forward, const math::Vec3& up) {
    m_forward = forward;
    m_up = up;
    const float ori[6] = { m_forward.x, m_forward.y, m_forward.z, m_up.x, m_up.y, m_up.z };
    alListenerfv(AL_ORIENTATION, ori);
}

void AudioListener::SetVelocity(const math::Vec3& velocity) {
    m_velocity = velocity;
    alListener3f(AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
}

} // namespace audio
} // namespace eyelib
