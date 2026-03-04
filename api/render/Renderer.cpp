#include "api/render/Renderer.h"

#include "eyelib/render/Renderer.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace api {

class Renderer::Impl {
public:
    eyelib::render::Renderer r{};
};

Renderer::Renderer() : m_impl(new Impl()) {}

void Renderer::SetDevice(RenderDevice device) {
    m_impl->r.SetDevice(static_cast<eyelib::render::rhi::IDevice*>(device.Get()));
}

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

