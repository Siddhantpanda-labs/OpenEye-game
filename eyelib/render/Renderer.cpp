#include "eyelib/render/Renderer.h"

#include "eyelib/core/Log.h"

#include <glad/glad.h>

namespace eyelib {
namespace render {

bool Renderer::Initialize() {
    if (m_initialized) return true;
    // Assumes a valid OpenGL context is already current (created by EyelibPlatform::Window).
    m_initialized = true;
    core::Log::Info("Renderer initialized");
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) return;
    m_initialized = false;
    core::Log::Info("Renderer shutdown");
}

void Renderer::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Renderer::Draw(const Mesh& mesh, const Shader& shader) {
    if (!m_initialized) return;
    if (!mesh.IsValid() || !shader.IsValid()) return;
    shader.Bind();
    mesh.Draw();
    shader.Unbind();
}

} // namespace render
} // namespace eyelib

