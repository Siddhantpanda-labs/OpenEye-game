#pragma once

/**
 * @brief Threading API. Re-exports eyelib::platform::threading.
 */
#include "eyelib/platform/threading/Mutex.h"
#include "eyelib/platform/threading/ConditionVariable.h"
#include "eyelib/platform/threading/Atomic.h"
#include "eyelib/platform/threading/Thread.h"
#include "eyelib/platform/threading/ThreadLocal.h"

namespace api {
namespace platform {
namespace threading {

using eyelib::platform::threading::Mutex;
using eyelib::platform::threading::LockGuard;
using eyelib::platform::threading::UniqueLock;
using eyelib::platform::threading::ConditionVariable;
using eyelib::platform::threading::Atomic;
using eyelib::platform::threading::Thread;
using eyelib::platform::threading::ThreadLocal;

} // namespace threading
} // namespace platform
} // namespace api
