#pragma once

/**
 * @brief Platform time API. Re-exports HighResTimer and FramePacer.
 */
#include "eyelib/platform/time/HighResTimer.h"
#include "eyelib/platform/time/FramePacer.h"

namespace api {
namespace platform {
namespace time {

using eyelib::platform::time::HighResTimer;
using eyelib::platform::time::FramePacer;

} // namespace time
} // namespace platform
} // namespace api
