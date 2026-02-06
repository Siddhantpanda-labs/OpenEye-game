#include "eyelib/render/ShadowMap.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eyelib {
namespace render {

ShadowMap::ShadowMap() = default;

ShadowMap::~ShadowMap() { Destroy(); }

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_resolution(other.m_resolution), m_fbo(other.m_fbo), m_depthTexture(other.m_depthTexture),
      m_deviceRHI(other.m_deviceRHI), m_depthTextureRHI(other.m_depthTextureRHI),
      m_lightView(other.m_lightView), m_lightProj(other.m_lightProj), m_shadowMatrix(other.m_shadowMatrix) {
    other.m_fbo = other.m_depthTexture = 0;
    other.m_deviceRHI = nullptr;
    other.m_depthTextureRHI = nullptr;
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_resolution = other.m_resolution;
        m_fbo = other.m_fbo;
        m_depthTexture = other.m_depthTexture;
        m_deviceRHI = other.m_deviceRHI;
        m_depthTextureRHI = other.m_depthTextureRHI;
        m_lightView = other.m_lightView;
        m_lightProj = other.m_lightProj;
        m_shadowMatrix = other.m_shadowMatrix;
        other.m_fbo = other.m_depthTexture = 0;
        other.m_deviceRHI = nullptr;
        other.m_depthTextureRHI = nullptr;
    }
    return *this;
}

bool ShadowMap::Initialize(uint32_t resolution) {
    return Initialize(nullptr, resolution);
}

bool ShadowMap::Initialize(rhi::IDevice* device, uint32_t resolution) {
    if (!resolution) return false;
    Destroy();
    m_resolution = resolution;
    m_deviceRHI = device;

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
    if (!ok) { Destroy(); return false; }

    if (device && m_depthTexture) {
        m_depthTextureRHI = device->CreateTextureWrapper(
            reinterpret_cast<void*>(static_cast<uintptr_t>(m_depthTexture)),
            m_resolution, m_resolution, rhi::RHITextureFormat::Depth24);
    }
    return ok;
}

void ShadowMap::UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                               const math::Vec3& cameraPos, float nearPlane, float farPlane) {
    (void)view;
    (void)proj;
    glm::vec3 dir(lightDir.x, lightDir.y, lightDir.z);
    float len = glm::length(dir);
    if (len < 1e-6f) len = 1.f;
    dir /= len;
    // Scene center: use camera position so shadow map follows the view
    glm::vec3 center(cameraPos.x, cameraPos.y, cameraPos.z);
    // Light "camera" is in the direction the light comes from, looking at the scene
    const float lightDistance = 80.f;
    glm::vec3 lightEye = center - dir * lightDistance;
    // Up vector: perpendicular to light dir so view is stable (e.g. light from +Y -> up = +Z)
    glm::vec3 up(0.f, 1.f, 0.f);
    if (std::abs(glm::dot(up, dir)) > 0.99f) up = glm::vec3(0.f, 0.f, 1.f);
    glm::mat4 lightView = glm::lookAt(lightEye, center, up);
    float extent = 55.f;
    // Fixed depth range for good precision; light view already covers scene around camera
    const float orthoNear = 1.f, orthoFar = 200.f;
    glm::mat4 lightProj = glm::ortho(-extent, extent, -extent, extent, orthoNear, orthoFar);
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
    if (m_deviceRHI && m_depthTextureRHI) {
        m_deviceRHI->DestroyTexture(m_depthTextureRHI);
        m_depthTextureRHI = nullptr;
    }
    m_deviceRHI = nullptr;
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_depthTexture) { glDeleteTextures(1, &m_depthTexture); m_depthTexture = 0; }
}

} // namespace render
} // namespace eyelib
