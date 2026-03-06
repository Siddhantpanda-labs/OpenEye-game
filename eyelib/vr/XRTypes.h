#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>

namespace eyelib {
namespace vr {

/** Per-view data from the XR runtime (pose + field of view). */
struct XRViewData {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    eyelib::math::Quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    /** Field of view: radians left/right/up/down from center. */
    float angleLeft = 0.0f;
    float angleRight = 0.0f;
    float angleUp = 0.0f;
    float angleDown = 0.0f;
};

/** Opaque handle for a submitted texture per view (e.g. OpenGL texture name). */
using XRTextureHandle = uint64_t;

} // namespace vr
} // namespace eyelib
