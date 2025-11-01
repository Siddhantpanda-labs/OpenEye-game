#include "Time.h"
#include <GLFW/glfw3.h>

namespace OpenEye {
namespace Core {

float Time::s_deltaTime = 0.0f;
float Time::s_totalTime = 0.0f;
uint64_t Time::s_frameCount = 0;
float Time::s_fps = 0.0f;
float Time::s_fixedDeltaTime = 1.0f / 60.0f; // 60 Hz by default
double Time::s_lastTime = 0.0;
double Time::s_fpsAccumulator = 0.0;
int Time::s_fpsFrames = 0;

void Time::Initialize() {
    s_lastTime = glfwGetTime();
    s_deltaTime = 0.0f;
    s_totalTime = 0.0f;
    s_frameCount = 0;
    s_fps = 0.0f;
    s_fpsAccumulator = 0.0;
    s_fpsFrames = 0;
}

void Time::Update() {
    double currentTime = glfwGetTime();
    s_deltaTime = static_cast<float>(currentTime - s_lastTime);
    s_lastTime = currentTime;
    
    s_totalTime += s_deltaTime;
    s_frameCount++;
    
    // Calculate FPS (average over 1 second)
    s_fpsAccumulator += s_deltaTime;
    s_fpsFrames++;
    
    if (s_fpsAccumulator >= 1.0) {
        s_fps = static_cast<float>(s_fpsFrames) / static_cast<float>(s_fpsAccumulator);
        s_fpsAccumulator = 0.0;
        s_fpsFrames = 0;
    }
}

} // namespace Core
} // namespace OpenEye

