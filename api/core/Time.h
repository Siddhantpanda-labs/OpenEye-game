#pragma once

#include <cstdint>

namespace eyelib {
namespace api {

/**
 * @brief Time management API
 * 
 * Provides frame timing, delta time, and fixed timestep support.
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
    static float DeltaTime();
    
    /**
     * @brief Get total time since initialization
     * @return Total time in seconds
     */
    static float TotalTime();
    
    /**
     * @brief Get frame count since initialization
     * @return Frame count
     */
    static uint64_t FrameCount();
    
    /**
     * @brief Get current FPS (averaged over 1 second)
     * @return Frames per second
     */
    static float FPS();
    
    /**
     * @brief Get fixed timestep delta time
     * @return Fixed delta time in seconds
     */
    static float FixedDeltaTime();
    
    /**
     * @brief Set fixed timestep delta time
     * @param dt Fixed delta time in seconds
     */
    static void SetFixedDeltaTime(float dt);
};

} // namespace api
} // namespace eyelib
