#pragma once

/**
 * @file All.h
 * @brief Convenience include for platform API.
 */

#include "api/platform/Window.h"
#include "api/platform/Input.h"
#include "api/platform/FileSystem.h"
#include "api/platform/Threading.h"
#include "api/platform/Time.h"

namespace eyelib {
namespace api {
namespace platform {
using ::eyelib::api::Window;
using ::eyelib::api::WindowConfig;
using ::eyelib::api::Input;
using ::eyelib::api::Vec2f;
namespace filesystem = ::api::platform::filesystem;
namespace time = ::api::platform::time;
} // namespace platform
} // namespace api
} // namespace eyelib
