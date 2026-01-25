#pragma once

#include "eyelib/audio/AudioClip.h"
#include "eyelib/audio/AudioSource.h"
#include "eyelib/math/Types.h"

#include <memory>
#include <string>

namespace eyelib {
namespace ecs {
namespace components {

struct AudioSource {
    audio::AudioSource source{};
    std::shared_ptr<audio::AudioClip> clip{};
    bool playOnStart = true;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;

    bool streaming = false;
    std::string streamPath{};
    bool streamLoop = false;

    bool started = false;
    math::Vec3 lastPosition{0.0f};
    bool hasLast = false;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
