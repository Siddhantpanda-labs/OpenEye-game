#pragma once

#include <mutex>

namespace eyelib {
namespace platform {
namespace threading {

/** Mutex wrapper. */
using Mutex = std::mutex;

/** RAII lock guard. */
template <typename M>
using LockGuard = std::lock_guard<M>;

/** Unique lock for use with condition variables. */
template <typename M>
using UniqueLock = std::unique_lock<M>;

} // namespace threading
} // namespace platform
} // namespace eyelib
