#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace audio { class AudioListener; }
namespace api {

class AudioListener {
public:
    AudioListener();
    ~AudioListener();

    AudioListener(const AudioListener&) = delete;
    AudioListener& operator=(const AudioListener&) = delete;

    void SetPosition(const eyelib::math::Vec3& position);
    void SetOrientation(const eyelib::math::Vec3& forward, const eyelib::math::Vec3& up);
    void SetVelocity(const eyelib::math::Vec3& velocity);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib
