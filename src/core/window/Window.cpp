#include "Window.h"
#include "core/log/Log.h"
#include <stdexcept>

namespace OpenEye {
namespace Core {

Window::Window() {
    // Initialize GLFW if not already done
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

Window::~Window() {
    Shutdown();
}

bool Window::Initialize(const WindowConfig& config) {
    m_config = config;
    
    // Set GLFW hints for OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajorVersion);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinorVersion);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    
    if (config.debugContext) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    }
    
    // Create window
    GLFWmonitor* monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), monitor, nullptr);
    
    if (!m_window) {
        OE_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    
    // Initialize GLAD
    if (!InitializeGL()) {
        OE_ERROR("Failed to initialize OpenGL");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }
    
    // Set VSync
    glfwSwapInterval(config.vsync ? 1 : 0);
    
    // Enable sRGB framebuffer if requested
    if (config.srgbFramebuffer) {
        glEnable(GL_FRAMEBUFFER_SRGB);
        OE_INFO("sRGB framebuffer enabled");
    }
    
    // Get actual framebuffer size
    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    
    // Setup callbacks
    SetupCallbacks();
    
    OE_INFO_F("Window created: %dx%d (OpenGL %d.%d)", m_width, m_height, config.glMajorVersion, config.glMinorVersion);
    
    return true;
}

void Window::Shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void Window::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

bool Window::ShouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void Window::SetShouldClose(bool value) {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, value ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool Window::IsKeyPressed(int key) const {
    if (!m_window) return false;
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool Window::IsMouseButtonPressed(int button) const {
    if (!m_window) return false;
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

void Window::GetMousePosition(double& x, double& y) const {
    if (m_window) {
        glfwGetCursorPos(m_window, &x, &y);
    } else {
        x = y = 0.0;
    }
}

void Window::SetMousePosition(double x, double y) {
    if (m_window) {
        glfwSetCursorPos(m_window, x, y);
    }
}

void Window::SetMouseVisible(bool visible) {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

void Window::SetMouseLocked(bool locked) {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->m_width = width;
        win->m_height = height;
        glViewport(0, 0, width, height);
        if (win->m_resizeCallback) {
            win->m_resizeCallback(width, height);
        }
    }
}

void Window::OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win && win->m_keyCallback) {
        win->m_keyCallback(key, scancode, action, mods);
    }
}

void Window::OnMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win && win->m_mouseButtonCallback) {
        win->m_mouseButtonCallback(button, action, mods);
    }
}

void Window::OnMouseMoveCallback(GLFWwindow* window, double x, double y) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win && win->m_mouseMoveCallback) {
        win->m_mouseMoveCallback(x, y);
    }
}

void Window::OnScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win && win->m_scrollCallback) {
        win->m_scrollCallback(xoffset, yoffset);
    }
}

void Window::GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                             GLsizei length, const GLchar* message, const void* userParam) {
    // Ignore certain non-critical messages
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
        return;
    }
    
    const char* sourceStr = "Unknown";
    const char* typeStr = "Unknown";
    const char* severityStr = "Unknown";
    
    switch (source) {
        case GL_DEBUG_SOURCE_API: sourceStr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceStr = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: sourceStr = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION: sourceStr = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER: sourceStr = "Other"; break;
    }
    
    switch (type) {
        case GL_DEBUG_TYPE_ERROR: typeStr = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeStr = "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY: typeStr = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: typeStr = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER: typeStr = "Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP: typeStr = "Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP: typeStr = "Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER: typeStr = "Other"; break;
    }
    
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: severityStr = "High"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; break;
        case GL_DEBUG_SEVERITY_LOW: severityStr = "Low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
    }
    
    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) {
        OE_ERROR_F("OpenGL Debug [%s][%s][%s] (ID: %u): %s", 
                   sourceStr, typeStr, severityStr, id, message);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        OE_WARN_F("OpenGL Debug [%s][%s][%s] (ID: %u): %s", 
                  sourceStr, typeStr, severityStr, id, message);
    } else {
        OE_DEBUG_F("OpenGL Debug [%s][%s][%s] (ID: %u): %s", 
                   sourceStr, typeStr, severityStr, id, message);
    }
}

void Window::SetupCallbacks() {
    if (!m_window) return;
    
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetKeyCallback(m_window, OnKeyCallback);
    glfwSetMouseButtonCallback(m_window, OnMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, OnMouseMoveCallback);
    glfwSetScrollCallback(m_window, OnScrollCallback);
}

bool Window::InitializeGL() {
    if (m_glInitialized) {
        return true;
    }
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        OE_ERROR("Failed to initialize GLAD");
        return false;
    }
    
    // Enable debug output if available
    if (m_config.debugContext) {
        // Try to enable debug output (available in OpenGL 4.3+ or via KHR_debug extension)
        if (glDebugMessageCallback) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDebugCallback, nullptr);
            
            // Enable all debug messages
            if (glDebugMessageControl) {
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            }
            
            OE_INFO("OpenGL debug context enabled");
        } else {
            OE_WARN("OpenGL debug context requested but not available");
        }
    }
    
    // Log OpenGL information
    OE_INFO_F("OpenGL Renderer: %s", glGetString(GL_RENDERER));
    OE_INFO_F("OpenGL Version: %s", glGetString(GL_VERSION));
    OE_INFO_F("OpenGL Vendor: %s", glGetString(GL_VENDOR));
    OE_INFO_F("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Face culling disabled (can cause issues with some models)
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    // glFrontFace(GL_CCW);
    
    m_glInitialized = true;
    return true;
}

} // namespace Core
} // namespace OpenEye

