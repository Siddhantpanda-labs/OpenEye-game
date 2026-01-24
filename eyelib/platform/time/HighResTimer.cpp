#include "platform/time/HighResTimer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <chrono>
#include <time.h>
#endif

namespace eyelib {
namespace platform {
namespace time {

#ifdef _WIN32
struct HighResTimer::Impl {
    LARGE_INTEGER freq{};
    Impl() { QueryPerformanceFrequency(&freq); }
};

HighResTimer::HighResTimer() : m_impl(new Impl) {}

HighResTimer::~HighResTimer() {
    delete m_impl;
    m_impl = nullptr;
}

uint64_t HighResTimer::GetTicks() const {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    if (m_impl->freq.QuadPart <= 0) return 0;
    return static_cast<uint64_t>((counter.QuadPart * 1000000000ULL) / m_impl->freq.QuadPart);
}

double HighResTimer::TicksToSeconds(uint64_t ticks) {
    return static_cast<double>(ticks) / 1e9;
}

double HighResTimer::TicksToMilliseconds(uint64_t ticks) {
    return static_cast<double>(ticks) / 1e6;
}
#else
struct HighResTimer::Impl {};

HighResTimer::HighResTimer() : m_impl(new Impl) {}

HighResTimer::~HighResTimer() {
    delete m_impl;
    m_impl = nullptr;
}

uint64_t HighResTimer::GetTicks() const {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

double HighResTimer::TicksToSeconds(uint64_t ticks) {
    return static_cast<double>(ticks) / 1e9;
}

double HighResTimer::TicksToMilliseconds(uint64_t ticks) {
    return static_cast<double>(ticks) / 1e6;
}
#endif

} // namespace time
} // namespace platform
} // namespace eyelib
