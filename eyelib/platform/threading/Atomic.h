#pragma once

#include <atomic>

namespace eyelib {
namespace platform {
namespace threading {

/** Atomic operations wrapper. */
template <typename T>
using Atomic = std::atomic<T>;

} // namespace threading
} // namespace platform
} // namespace eyelib
