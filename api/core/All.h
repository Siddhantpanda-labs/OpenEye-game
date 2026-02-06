#pragma once

/**
 * @file All.h
 * @brief Convenience header that includes all Core API headers
 * 
 * Client applications can include this single header to get all Core functionality:
 * @code
 * #include "api/core/All.h"
 * @endcode
 */

#include "Engine.h"
#include "Time.h"
#include "Log.h"
#include "Memory.h"
#include "FrameAllocator.h"
#include "FrameAllocatorAdaptor.h"
#include "Job.h"
#include "Plugin.h"

namespace eyelib {
namespace api {
namespace core {
using ::eyelib::api::Engine;
using ::eyelib::api::Time;
using ::eyelib::api::Log;
using ::eyelib::api::LogLevel;
using ::api::ThreadPool;
using ::api::TaskGraph;
using ::api::ParallelFor;
using ::api::ParallelForEach;
using ::api::FrameAllocator;
namespace memory = ::api::memory;
namespace plugin = ::api::plugin;
} // namespace core
} // namespace api
} // namespace eyelib
