#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace audio {

/**
 * @brief 3D audio listener state.
 */
class AudioListener {
public:
    AudioListener();
    void SetPosition(const math::Vec3& position);
    void SetOrientation(const math::Vec3& forward, const math::Vec3& up);
    void SetVelocity(const math::Vec3& velocity);

    math::Vec3 GetPosition() const { return m_position; }
    math::Vec3 GetForward() const { return m_forward; }
    math::Vec3 GetUp() const { return m_up; }
    math::Vec3 GetVelocity() const { return m_velocity; }

private:
    math::Vec3 m_position{0.0f};
    math::Vec3 m_forward{0.0f, 0.0f, -1.0f};
    math::Vec3 m_up{0.0f, 1.0f, 0.0f};
    math::Vec3 m_velocity{0.0f};
};

} // namespace audio
} // namespace eyelib
