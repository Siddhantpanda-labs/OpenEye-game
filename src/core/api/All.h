#pragma once

/**
 * @brief Convenience header that includes all Core API headers
 * 
 * Apps can include this single header to get all Core functionality:
 * #include "core/api/All.h"
 */

#include "../window/Window.h"
#include "../input/Input.h"
#include "../time/Time.h"
#include "../log/Log.h"

namespace OpenEye {
namespace Core {
namespace API {

using Window = OpenEye::Core::Window;
using Input = OpenEye::Core::Input;
using Time = OpenEye::Core::Time;
using Log = OpenEye::Core::Log;

} // namespace API
} // namespace Core
} // namespace OpenEye

