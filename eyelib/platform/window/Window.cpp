#include "platform/window/Window.h"

#include "core/Log.h"
#include "platform/input/Input.h"

// IMPORTANT: GLAD must be included before GLFW to prevent OpenGL header conflicts
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <stdexcept>

namespace {

// Ensure glfwInit/glfwTerminate are managed safely for multiple windows.
struct GlfwRuntime {
    GlfwRuntime() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }
    ~GlfwRuntime() {
        glfwTerminate();
    }
};

GlfwRuntime& GetGlfwRuntime() {
    static GlfwRuntime runtime;
    return runtime;
}

// OpenGL debug callback (implementation-only; not exposed in headers)
void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                              GLsizei /*length*/, const GLchar* message, const void* /*userParam*/) {
    // Ignore common non-critical messages
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
        return;
    }

    const char* severityStr = "Unknown";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: severityStr = "High"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; break;
        case GL_DEBUG_SEVERITY_LOW: severityStr = "Low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
    }

    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) {
        eyelib::core::Log::ErrorF("OpenGL Debug [%s] (ID: %u): %s", severityStr, id, message);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        eyelib::core::Log::WarnF("OpenGL Debug [%s] (ID: %u): %s", severityStr, id, message);
    } else {
        eyelib::core::Log::DebugF("OpenGL Debug [%s] (ID: %u): %s", severityStr, id, message);
    }
}

} // namespace

namespace eyelib {
namespace platform {

Window::Window() = default;

Window::~Window() {
    Shutdown();
}

bool Window::Initialize(const WindowConfig& config) {
    (void)GetGlfwRuntime(); // ensure GLFW is initialized

    m_config = config;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajorVersion);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinorVersion);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    if (config.debugContext) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    }

    GLFWmonitor* monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), monitor, nullptr);

    if (!m_window) {
        core::Log::Error("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!InitializeGL()) {
        core::Log::Error("Failed to initialize OpenGL");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    glfwSwapInterval(config.vsync ? 1 : 0);

    if (config.srgbFramebuffer) {
        glEnable(GL_FRAMEBUFFER_SRGB);
        core::Log::Info("sRGB framebuffer enabled");
    }

    glfwGetFramebufferSize(m_window, &m_width, &m_height);

    SetupCallbacks();

    core::Log::InfoF("Window created: %dx%d (OpenGL %d.%d)", m_width, m_height, config.glMajorVersion, config.glMinorVersion);
    return true;
}

void Window::Shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::PollEvents() {
    // advance input state once per frame (just-pressed/released)
    Input::Update();
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

float Window::AspectRatio() const {
    return (m_height == 0) ? 0.0f : static_cast<float>(m_width) / static_cast<float>(m_height);
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    win->m_width = width;
    win->m_height = height;
    glViewport(0, 0, width, height);

    if (win->m_resizeCallback) {
        win->m_resizeCallback(width, height);
    }
}

void Window::OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    Input::OnKeyEvent(key, scancode, action, mods);

    if (win->m_keyCallback) {
        win->m_keyCallback(key, scancode, action, mods);
    }
}

void Window::OnMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    Input::OnMouseButtonEvent(button, action, mods);

    if (win->m_mouseButtonCallback) {
        win->m_mouseButtonCallback(button, action, mods);
    }
}

void Window::OnMouseMoveCallback(GLFWwindow* window, double x, double y) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    Input::OnMouseMoveEvent(x, y);

    if (win->m_mouseMoveCallback) {
        win->m_mouseMoveCallback(x, y);
    }
}

void Window::OnScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    Input::OnScrollEvent(xoffset, yoffset);

    if (win->m_scrollCallback) {
        win->m_scrollCallback(xoffset, yoffset);
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
    if (m_glInitialized) return true;

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        core::Log::Error("Failed to initialize GLAD");
        return false;
    }

    if (m_config.debugContext) {
        if (glDebugMessageCallback) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDebugCallback, nullptr);
            if (glDebugMessageControl) {
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            }
            core::Log::Info("OpenGL debug context enabled");
        } else {
            core::Log::Warn("OpenGL debug context requested but not available");
        }
    }

    core::Log::InfoF("OpenGL Renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    core::Log::InfoF("OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    core::Log::InfoF("OpenGL Vendor: %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    core::Log::InfoF("GLSL Version: %s", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    m_glInitialized = true;
    return true;
}

} // namespace platform
} // namespace eyelib

