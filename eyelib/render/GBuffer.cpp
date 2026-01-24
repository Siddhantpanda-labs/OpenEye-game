#include "eyelib/render/GBuffer.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

GBuffer::GBuffer() = default;

GBuffer::~GBuffer() { Destroy(); }

GBuffer::GBuffer(GBuffer&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height),
      m_fbo(other.m_fbo),
      m_positionTexture(other.m_positionTexture),
      m_normalTexture(other.m_normalTexture),
      m_albedoTexture(other.m_albedoTexture),
      m_pbrParamsTexture(other.m_pbrParamsTexture),
      m_velocityTexture(other.m_velocityTexture),
      m_depthTexture(other.m_depthTexture) {
    other.m_fbo = other.m_positionTexture = other.m_normalTexture = 0;
    other.m_albedoTexture = other.m_pbrParamsTexture = other.m_velocityTexture = 0;
    other.m_depthTexture = 0;
}

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_width = other.m_width; m_height = other.m_height;
        m_fbo = other.m_fbo;
        m_positionTexture = other.m_positionTexture;
        m_normalTexture = other.m_normalTexture;
        m_albedoTexture = other.m_albedoTexture;
        m_pbrParamsTexture = other.m_pbrParamsTexture;
        m_velocityTexture = other.m_velocityTexture;
        m_depthTexture = other.m_depthTexture;
        other.m_fbo = other.m_positionTexture = other.m_normalTexture = 0;
        other.m_albedoTexture = other.m_pbrParamsTexture = other.m_velocityTexture = 0;
        other.m_depthTexture = 0;
    }
    return *this;
}

bool GBuffer::Initialize(uint32_t width, uint32_t height) {
    if (!width || !height) return false;
    Destroy();
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    CreateTextures();
    AttachTextures();

    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
    glDrawBuffers(5, drawBufs);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Destroy();
        return false;
    }
    return true;
}

void GBuffer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    Initialize(width, height);
}

void GBuffer::CreateTextures() {
    auto makeTex = [](unsigned int& id, GLenum ifmt, GLenum fmt, GLenum type, int w, int h) {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    makeTex(m_positionTexture, GL_RGB16F, GL_RGB, GL_FLOAT, m_width, m_height);
    makeTex(m_normalTexture, GL_RGB16F, GL_RGB, GL_FLOAT, m_width, m_height);
    makeTex(m_albedoTexture, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, m_width, m_height);
    makeTex(m_pbrParamsTexture, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, m_width, m_height);
    makeTex(m_velocityTexture, GL_RG16F, GL_RG, GL_FLOAT, m_width, m_height);

    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float bc[] = {1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bc);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GBuffer::AttachTextures() {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_positionTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normalTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_albedoTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_pbrParamsTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, m_velocityTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
}

void GBuffer::BindForGeometryPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GBuffer::BindForLightingPass() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_positionTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_normalTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_albedoTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_pbrParamsTexture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
}

void GBuffer::BindDepthTexture(unsigned int textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
}

void GBuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::Destroy() {
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_positionTexture) { glDeleteTextures(1, &m_positionTexture); m_positionTexture = 0; }
    if (m_normalTexture) { glDeleteTextures(1, &m_normalTexture); m_normalTexture = 0; }
    if (m_albedoTexture) { glDeleteTextures(1, &m_albedoTexture); m_albedoTexture = 0; }
    if (m_pbrParamsTexture) { glDeleteTextures(1, &m_pbrParamsTexture); m_pbrParamsTexture = 0; }
    if (m_velocityTexture) { glDeleteTextures(1, &m_velocityTexture); m_velocityTexture = 0; }
    if (m_depthTexture) { glDeleteTextures(1, &m_depthTexture); m_depthTexture = 0; }
}

} // namespace render
} // namespace eyelib
