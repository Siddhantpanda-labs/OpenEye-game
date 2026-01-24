#pragma once

#include "platform/time/HighResTimer.h"
#include <cstdint>
#include <cmath>

namespace eyelib {
namespace platform {
namespace time {

/**
 * @brief Frame pacer. Maintains target FPS via WaitForNextFrame().
 * Uses HighResTimer for precise timing.
 */
class FramePacer {
public:
    FramePacer() = default;

    void SetTargetFPS(double fps) { m_targetFPS = (fps > 0.0) ? fps : 60.0; }
    double GetTargetFPS() const { return m_targetFPS; }

    /** Call at start of frame. Updates frame time; sleep until next frame if ahead. */
    void WaitForNextFrame();

    /** Actual last frame duration in seconds. */
    double GetFrameTime() const { return m_frameTimeSec; }

private:
    HighResTimer m_timer;
    double m_targetFPS = 60.0;
    double m_frameTimeSec = 0.0;
    uint64_t m_lastTick = 0;
    bool m_firstFrame = true;
};

} // namespace time
} // namespace platform
} // namespace eyelib
