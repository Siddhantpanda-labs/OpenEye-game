#include "api/render/Renderer.h"

#include "eyelib/render/Renderer.h"

namespace eyelib {
namespace api {

class Renderer::Impl {
public:
    eyelib::render::Renderer r{};
};

Renderer::Renderer() : m_impl(new Impl()) {}

Renderer::~Renderer() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool Renderer::Initialize() {
    return m_impl->r.Initialize();
}

void Renderer::Shutdown() {
    if (!m_impl) return;
    m_impl->r.Shutdown();
}

void Renderer::Clear(float r, float g, float b, float a) {
    m_impl->r.Clear(r, g, b, a);
}

} // namespace api
} // namespace eyelib

