#include "api/platform/Window.h"

#include "core/Log.h"
#include "eyelib/platform/window/Window.h"

namespace eyelib {
namespace api {

class Window::Impl {
public:
    eyelib::platform::Window window{};
};

Window::Window() : m_impl(new Impl()) {}

Window::~Window() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool Window::Initialize(const WindowConfig& config) {
    if (!m_impl) return false;

    eyelib::platform::WindowConfig cfg{};
    cfg.title = config.title;
    cfg.width = config.width;
    cfg.height = config.height;
    cfg.fullscreen = config.fullscreen;
    cfg.vsync = config.vsync;
    cfg.resizable = config.resizable;
    cfg.srgbFramebuffer = config.srgbFramebuffer;
    cfg.debugContext = config.debugContext;
    cfg.glMajorVersion = config.glMajorVersion;
    cfg.glMinorVersion = config.glMinorVersion;

    return m_impl->window.Initialize(cfg);
}

void Window::Shutdown() {
    if (!m_impl) return;
    m_impl->window.Shutdown();
}

void Window::PollEvents() {
    if (!m_impl) return;
    m_impl->window.PollEvents();
}

void Window::SwapBuffers() {
    if (!m_impl) return;
    m_impl->window.SwapBuffers();
}

void Window::Clear(float r, float g, float b, float a) {
    if (!m_impl) return;
    m_impl->window.Clear(r, g, b, a);
}

bool Window::ShouldClose() const {
    if (!m_impl) return true;
    return m_impl->window.ShouldClose();
}

void Window::SetShouldClose(bool value) {
    if (!m_impl) return;
    m_impl->window.SetShouldClose(value);
}

int Window::Width() const {
    if (!m_impl) return 0;
    return m_impl->window.Width();
}

int Window::Height() const {
    if (!m_impl) return 0;
    return m_impl->window.Height();
}

float Window::AspectRatio() const {
    if (!m_impl) return 0.0f;
    return m_impl->window.AspectRatio();
}

void* Window::GetNativeHandle() const {
    if (!m_impl) return nullptr;
    return m_impl->window.NativeHandle();
}

} // namespace api
} // namespace eyelib

