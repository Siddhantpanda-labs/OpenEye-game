#pragma once

#include <string>

namespace eyelib {
namespace api {

struct WindowConfig {
    std::string title = "OpenEye";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    bool srgbFramebuffer = true;
    bool debugContext = true;
    int glMajorVersion = 4;
    int glMinorVersion = 5;
};

/**
 * @brief Client-facing window wrapper (no GLFW/OpenGL types exposed).
 */
class Window {
public:
    Window();
    ~Window();

    bool Initialize(const WindowConfig& config = WindowConfig{});
    void Shutdown();

    void PollEvents();
    void SwapBuffers();
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);

    bool ShouldClose() const;
    void SetShouldClose(bool value);

    int Width() const;
    int Height() const;
    float AspectRatio() const;

    /** @brief Native window handle (e.g. GLFWwindow*) for ImGui etc. */
    void* GetNativeHandle() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

