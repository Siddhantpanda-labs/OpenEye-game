#pragma once

#include <cstdint>

namespace eyelib {
namespace platform {
namespace time {

/** High-resolution monotonic timer. Ticks are nanoseconds. */
class HighResTimer {
public:
    HighResTimer();
    ~HighResTimer();
    HighResTimer(const HighResTimer&) = delete;
    HighResTimer& operator=(const HighResTimer&) = delete;

    /** Current ticks (monotonic nanoseconds). */
    uint64_t GetTicks() const;

    static double TicksToSeconds(uint64_t ticks);
    static double TicksToMilliseconds(uint64_t ticks);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace time
} // namespace platform
} // namespace eyelib
