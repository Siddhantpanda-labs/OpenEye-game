#include "SkyboxRenderer.h"
#include "resources/Shader.h"
#include "FullscreenQuad.h"
#include "Camera.h"
#include "resources/Cubemap.h"
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

SkyboxRenderer::SkyboxRenderer() = default;
SkyboxRenderer::~SkyboxRenderer() {
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
}

bool SkyboxRenderer::Initialize(uint32_t width, uint32_t height) {
    m_width = width; m_height = height;
    CreateTargets();
    m_shader = std::make_shared<Shader>();
    m_shader->LoadFromFile("assets/shaders/passes/sky/skybox.vert", "assets/shaders/passes/sky/skybox.frag");
    return true;
}

void SkyboxRenderer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    m_width = width; m_height = height;
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    CreateTargets();
}

void SkyboxRenderer::CreateTargets() {
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SkyboxRenderer::Render(const Camera& camera, const Cubemap& cubemap) {
    if (!m_shader || !cubemap.IsValid()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader->Bind();
    m_shader->SetInt("u_Skybox", 0);
    cubemap.Bind(0);

    static FullscreenQuad quad;
    quad.Render();

    m_shader->Unbind();
    cubemap.Unbind(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

void SkyboxRenderer::RenderSolidColor(const glm::vec3& skyColor) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}

} // namespace Rendering
} // namespace OpenEye


