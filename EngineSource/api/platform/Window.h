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
    /** When true, create window with GLFW_NO_API (Vulkan/Diligent surface); no GL context, no GLAD/SwapBuffers. */
    bool useNativeApiOnly = false;
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
    /** Make the GL context current on the calling thread. Use from render thread after startup. */
    void MakeContextCurrent();
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);

    /** Query current framebuffer size (thread-safe). Call from render thread before render. */
    void RefreshFramebufferSize();
    /** Re-apply vsync. Call from render thread each frame with context current. */
    void ApplyVsync();

    bool ShouldClose() const;
    void SetShouldClose(bool value);

    int Width() const;
    int Height() const;
    float AspectRatio() const;

    /** @brief Native window handle (e.g. GLFWwindow*) for ImGui etc. */
    void* GetNativeHandle() const;

    /** @brief Platform window pointer (for RHI device creation). Cast to platform::Window* when calling CreateGraphicsDevice. */
    void* GetPlatformWindow();

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

