#include "api/rendering/Renderer.h"

#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace api {
namespace rendering {

class Renderer::Impl {
public:
    eyelib::rendering::rhi::IDevice* device = nullptr;
};

Renderer::Renderer() : m_impl(new Impl()) {}

Renderer::~Renderer() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

void Renderer::SetDevice(RenderDevice device) {
    if (!m_impl) return;
    m_impl->device = static_cast<eyelib::rendering::rhi::IDevice*>(device.Get());
}

bool Renderer::Initialize() {
    if (!m_impl) return false;
    // No per-renderer GPU state in v1: the device, swapchain, frame graph and features
    // are owned by RenderPipeline (M1.4). This call returns true so existing editor/player
    // code paths that gate on `Initialize()` continue to flow.
    return true;
}

void Renderer::Shutdown() {
    if (!m_impl) return;
    m_impl->device = nullptr;
}

void Renderer::Clear(float r, float g, float b, float a) {
    if (!m_impl || !m_impl->device) return;
    eyelib::rendering::rhi::ICommandList* cmd = m_impl->device->GetActiveRecordingCommandList();
    if (!cmd) return;
    eyelib::rendering::rhi::ClearValue clear{};
    clear.color[0] = r;
    clear.color[1] = g;
    clear.color[2] = b;
    clear.color[3] = a;
    cmd->ClearDefaultFramebuffer(clear);
}

} // namespace rendering
} // namespace api
} // namespace eyelib
