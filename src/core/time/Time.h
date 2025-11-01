#pragma once

#include <cstdint>

namespace OpenEye {
namespace Core {

class Time {
public:
    static void Initialize();
    static void Update();
    
    // Get frame time (delta time since last frame)
    static float DeltaTime() { return s_deltaTime; }
    
    // Get total time since initialization
    static float TotalTime() { return s_totalTime; }
    
    // Get frame count
    static uint64_t FrameCount() { return s_frameCount; }
    
    // Get FPS
    static float FPS() { return s_fps; }
    
    // Fixed timestep helpers
    static float FixedDeltaTime() { return s_fixedDeltaTime; }
    static void SetFixedDeltaTime(float dt) { s_fixedDeltaTime = dt; }

private:
    static float s_deltaTime;
    static float s_totalTime;
    static uint64_t s_frameCount;
    static float s_fps;
    static float s_fixedDeltaTime;
    static double s_lastTime;
    static double s_fpsAccumulator;
    static int s_fpsFrames;
};

} // namespace Core
} // namespace OpenEye

