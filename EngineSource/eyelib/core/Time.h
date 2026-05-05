#pragma once

#include <cstdint>

namespace eyelib {
namespace core {

/**
 * @brief Time management system
 * 
 * Provides frame timing, delta time, and fixed timestep support.
 * Must be updated each frame via Update().
 */
class Time {
public:
    /**
     * @brief Initialize the time system
     */
    static void Initialize();
    
    /**
     * @brief Update time (call once per frame)
     */
    static void Update();
    
    /**
     * @brief Get frame time (delta time since last frame)
     * @return Delta time in seconds
     */
    static float DeltaTime() { return s_deltaTime; }
    
    /**
     * @brief Get total time since initialization
     * @return Total time in seconds
     */
    static float TotalTime() { return s_totalTime; }
    
    /**
     * @brief Get frame count since initialization
     * @return Frame count
     */
    static uint64_t FrameCount() { return s_frameCount; }
    
    /**
     * @brief Get current FPS (averaged over 1 second)
     * @return Frames per second
     */
    static float FPS() { return s_fps; }
    
    /**
     * @brief Get fixed timestep delta time
     * @return Fixed delta time in seconds
     */
    static float FixedDeltaTime() { return s_fixedDeltaTime; }
    
    /**
     * @brief Set fixed timestep delta time
     * @param dt Fixed delta time in seconds
     */
    static void SetFixedDeltaTime(float dt) { s_fixedDeltaTime = dt; }

    /** Smoothed delta time (EMA) for stable display and tuning. Default alpha 0.1. */
    static float SmoothedDeltaTime() { return s_smoothedDeltaTime; }
    static void SetSmoothedDeltaAlpha(float alpha) { s_smoothedDeltaAlpha = (alpha > 0.f && alpha <= 1.f) ? alpha : 0.1f; }

private:
    static float s_deltaTime;
    static float s_totalTime;
    static uint64_t s_frameCount;
    static float s_fps;
    static float s_fixedDeltaTime;
    static double s_lastTime;
    static double s_fpsAccumulator;
    static int s_fpsFrames;
    static float s_smoothedDeltaTime;
    static float s_smoothedDeltaAlpha;
};

} // namespace core
} // namespace eyelib
