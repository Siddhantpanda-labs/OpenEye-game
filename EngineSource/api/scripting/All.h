#pragma once

/**
 * @brief Scripting API - Public interface for Lua scripting system.
 * 
 * This API provides a clean, stable interface to the scripting system
 * using PIMPL pattern to hide implementation details and maintain ABI stability.
 */

#include "api/scripting/Script.h"
#include "api/scripting/ScriptVM.h"

namespace eyelib {
namespace api {
namespace scripting {
    // Re-export for convenience
    using ScriptHandle = ::eyelib::scripting::ScriptHandle;
    constexpr ScriptHandle InvalidScriptHandle = ::eyelib::scripting::InvalidScriptHandle;
} // namespace scripting
} // namespace api
} // namespace eyelib
