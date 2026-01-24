#include "platform/time/FramePacer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <thread>
#include <chrono>
#endif

#include <cmath>
#include <algorithm>

namespace eyelib {
namespace platform {
namespace time {

namespace {
void SleepSeconds(double sec) {
    if (sec <= 0.0) return;
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(std::round(sec * 1000.0));
    if (ms > 0) Sleep(ms);
#else
    auto dur = std::chrono::duration<double>(sec);
    std::this_thread::sleep_for(dur);
#endif
}
} // namespace

void FramePacer::WaitForNextFrame() {
    const uint64_t now = m_timer.GetTicks();
    if (m_firstFrame) {
        m_lastTick = now;
        m_firstFrame = false;
        m_frameTimeSec = 0.0;
        return;
    }
    m_frameTimeSec = HighResTimer::TicksToSeconds(now - m_lastTick);
    m_lastTick = now;

    const double targetSec = 1.0 / m_targetFPS;
    const double sleepSec = targetSec - m_frameTimeSec;
    if (sleepSec > 0.0)
        SleepSeconds(sleepSec);
}

} // namespace time
} // namespace platform
} // namespace eyelib
