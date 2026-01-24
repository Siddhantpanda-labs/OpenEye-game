#include "eyelib/render/ShadowMap.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace render {

ShadowMap::ShadowMap() = default;

ShadowMap::~ShadowMap() { Destroy(); }

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_resolution(other.m_resolution), m_fbo(other.m_fbo), m_depthTexture(other.m_depthTexture),
      m_lightView(other.m_lightView), m_lightProj(other.m_lightProj), m_shadowMatrix(other.m_shadowMatrix) {
    other.m_fbo = other.m_depthTexture = 0;
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_resolution = other.m_resolution;
        m_fbo = other.m_fbo;
        m_depthTexture = other.m_depthTexture;
        m_lightView = other.m_lightView;
        m_lightProj = other.m_lightProj;
        m_shadowMatrix = other.m_shadowMatrix;
        other.m_fbo = other.m_depthTexture = 0;
    }
    return *this;
}

bool ShadowMap::Initialize(uint32_t resolution) {
    if (!resolution) return false;
    Destroy();
    m_resolution = resolution;

    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_resolution, m_resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float bc[] = {1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bc);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) Destroy();
    return ok;
}

void ShadowMap::UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                               const math::Vec3& cameraPos, float nearPlane, float farPlane) {
    (void)view;
    (void)proj;
    (void)nearPlane;
    (void)farPlane;
    glm::vec3 pos(cameraPos.x, cameraPos.y, cameraPos.z);
    glm::vec3 dir(lightDir.x, lightDir.y, lightDir.z);
    glm::mat4 lightView = glm::lookAt(pos, pos - dir, glm::vec3(0, 1, 0));
    float extent = 50.f;
    glm::mat4 lightProj = glm::ortho(-extent, extent, -extent, extent, 0.1f, 200.f);
    m_lightView = math::Mat4(lightView);
    m_lightProj = math::Mat4(lightProj);
    m_shadowMatrix = m_lightProj * m_lightView;
}

void ShadowMap::BindForShadowPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_resolution, m_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::Destroy() {
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_depthTexture) { glDeleteTextures(1, &m_depthTexture); m_depthTexture = 0; }
}

} // namespace render
} // namespace eyelib
