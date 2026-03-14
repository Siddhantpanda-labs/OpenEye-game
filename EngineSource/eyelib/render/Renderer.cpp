#include "eyelib/render/Renderer.h"

namespace eyelib {
namespace render {

bool Renderer::Initialize() {
    if (m_initialized) return true;
    // Assumes a valid OpenGL context is already current (created by EyelibPlatform::Window).
    m_initialized = true;
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) return;
    m_initialized = false;
}

void Renderer::Clear(float r, float g, float b, float a) {
    rhi::IDevice* device = m_injectedDevice;
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    rhi::ClearValue clear{};
    clear.color[0] = r;
    clear.color[1] = g;
    clear.color[2] = b;
    clear.color[3] = a;
    cmd->ClearDefaultFramebuffer(clear);
}

void Renderer::Draw(const Mesh& mesh, const Shader& shader) {
    if (!m_initialized || !m_injectedDevice) return;
    if (!mesh.IsValid() || !shader.IsValid()) return;
    shader.Bind(m_injectedDevice);
    mesh.Draw(m_injectedDevice);
    shader.Unbind();
}

} // namespace render
} // namespace eyelib

