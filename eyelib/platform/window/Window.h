#pragma once

#include <functional>
#include <string>

// Forward-declare GLFW type to avoid exposing GLFW headers here.
// NOTE: GLFW defines GLFWwindow in the global namespace.
struct GLFWwindow;

namespace eyelib {
namespace platform {

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
 * @brief GLFW + OpenGL window wrapper.
 *
 * Implementation uses GLFW + GLAD internally, but this header avoids
 * including GL/GLFW headers to keep compile times and exposure clean.
 *
 * Input integration: window callback events are also fed into
 * `eyelib::platform::Input` automatically.
 */
class Window {
public:
    using ResizeCallback = std::function<void(int width, int height)>;
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    using MouseMoveCallback = std::function<void(double x, double y)>;
    using ScrollCallback = std::function<void(double xoffset, double yoffset)>;

    Window();
    ~Window();

    bool Initialize(const WindowConfig& config = WindowConfig{});
    void Shutdown();

    void PollEvents();
    void SwapBuffers();
    /** Make the GL context current on the calling thread. Safe to call from any thread. */
    void MakeContextCurrent();
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);

    /** Query current framebuffer size from the system (thread-safe). Call from render thread before using size. Updates Width()/Height(). */
    void RefreshFramebufferSize();
    /** Re-apply vsync (swap interval). Call from render thread with context current; use every frame so driver cannot drop it. */
    void ApplyVsync();

    bool ShouldClose() const;
    void SetShouldClose(bool value);

    int Width() const { return m_width; }
    int Height() const { return m_height; }
    float AspectRatio() const;

    // Opaque access for internal engine integration (do not expose in api/)
    ::GLFWwindow* NativeHandle() const { return m_window; }

    // Callbacks
    /** Set resize callback. Contract: must not call GL or touch render resources; may run on main thread while GL context is on render thread. Prefer storing (width,height) and applying resize on render thread. */
    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }
    void SetKeyCallback(KeyCallback callback) { m_keyCallback = std::move(callback); }
    void SetMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallback = std::move(callback); }
    void SetMouseMoveCallback(MouseMoveCallback callback) { m_mouseMoveCallback = std::move(callback); }
    void SetScrollCallback(ScrollCallback callback) { m_scrollCallback = std::move(callback); }

private:
    static void FramebufferResizeCallback(::GLFWwindow* window, int width, int height);
    static void OnKeyCallback(::GLFWwindow* window, int key, int scancode, int action, int mods);
    static void OnMouseButtonCallback(::GLFWwindow* window, int button, int action, int mods);
    static void OnMouseMoveCallback(::GLFWwindow* window, double x, double y);
    static void OnScrollCallback(::GLFWwindow* window, double xoffset, double yoffset);

    void SetupCallbacks();
    bool InitializeGL();

private:
    ::GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;
    WindowConfig m_config{};

    ResizeCallback m_resizeCallback{};
    KeyCallback m_keyCallback{};
    MouseButtonCallback m_mouseButtonCallback{};
    MouseMoveCallback m_mouseMoveCallback{};
    ScrollCallback m_scrollCallback{};

    bool m_glInitialized = false;
};

} // namespace platform
} // namespace eyelib

