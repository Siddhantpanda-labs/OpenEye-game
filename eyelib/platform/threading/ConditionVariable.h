#pragma once

#include "platform/threading/Mutex.h"
#include <condition_variable>

namespace eyelib {
namespace platform {
namespace threading {

/** Condition variable. */
using ConditionVariable = std::condition_variable;

} // namespace threading
} // namespace platform
} // namespace eyelib
