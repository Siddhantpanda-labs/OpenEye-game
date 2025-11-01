#pragma once

#include <string>
#include <functional>

// IMPORTANT: GLAD must be included before GLFW to prevent OpenGL header conflicts
// Define this to prevent GLFW from including OpenGL headers
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace OpenEye {
namespace Core {

struct WindowConfig {
    std::string title = "OpenEye";
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    bool srgbFramebuffer = true;
    bool debugContext = true;
    int glMajorVersion = 4;
    int glMinorVersion = 5;
};

class Window {
public:
    using ResizeCallback = std::function<void(int width, int height)>;
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    using MouseMoveCallback = std::function<void(double x, double y)>;
    using ScrollCallback = std::function<void(double xoffset, double yoffset)>;

    Window();
    ~Window();
    
    bool Initialize(const WindowConfig& config = WindowConfig());
    void Shutdown();
    
    void PollEvents();
    void SwapBuffers();
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    
    bool ShouldClose() const;
    void SetShouldClose(bool value);
    
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    float GetAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    
    GLFWwindow* GetNativeHandle() const { return m_window; }
    
    // Callbacks
    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = callback; }
    void SetKeyCallback(KeyCallback callback) { m_keyCallback = callback; }
    void SetMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallback = callback; }
    void SetMouseMoveCallback(MouseMoveCallback callback) { m_mouseMoveCallback = callback; }
    void SetScrollCallback(ScrollCallback callback) { m_scrollCallback = callback; }
    
    // Input queries
    bool IsKeyPressed(int key) const;
    bool IsMouseButtonPressed(int button) const;
    void GetMousePosition(double& x, double& y) const;
    void SetMousePosition(double x, double y);
    void SetMouseVisible(bool visible);
    void SetMouseLocked(bool locked);

private:
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void OnMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void OnMouseMoveCallback(GLFWwindow* window, double x, double y);
    static void OnScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                 GLsizei length, const GLchar* message, const void* userParam);
    
    void SetupCallbacks();
    bool InitializeGL();
    
    GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;
    WindowConfig m_config;
    
    ResizeCallback m_resizeCallback;
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    ScrollCallback m_scrollCallback;
    
    bool m_glInitialized = false;
};

} // namespace Core
} // namespace OpenEye

