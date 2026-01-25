#pragma once

#include "eyelib/math/Types.h"

#include <string>

namespace eyelib {
namespace api {

class AudioClip;

struct AudioEventParams {
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
};

class AudioEvents {
public:
    AudioEvents();
    ~AudioEvents();

    AudioEvents(const AudioEvents&) = delete;
    AudioEvents& operator=(const AudioEvents&) = delete;

    void RegisterEvent(const std::string& name, const AudioClip& clip, const AudioEventParams& params = {});
    void TriggerEvent(const std::string& name, const eyelib::math::Vec3& position);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib
