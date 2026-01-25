#pragma once

#include "eyelib/audio/AudioListener.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace ecs {
namespace components {

struct AudioListener {
    audio::AudioListener listener{};
    bool active = true;
    math::Vec3 lastPosition{0.0f};
    bool hasLast = false;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
