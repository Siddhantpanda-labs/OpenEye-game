#include "GBuffer.h"
#include "core/log/Log.h"
#include <glad/glad.h>

namespace OpenEye {
namespace Rendering {

GBuffer::GBuffer() = default;

GBuffer::~GBuffer() {
    Destroy();
}

GBuffer::GBuffer(GBuffer&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_fbo(other.m_fbo)
    , m_positionTexture(other.m_positionTexture)
    , m_normalTexture(other.m_normalTexture)
    , m_albedoTexture(other.m_albedoTexture)
    , m_pbrParamsTexture(other.m_pbrParamsTexture)
    , m_depthTexture(other.m_depthTexture) {
    
    other.m_fbo = 0;
    other.m_positionTexture = 0;
    other.m_normalTexture = 0;
    other.m_albedoTexture = 0;
    other.m_pbrParamsTexture = 0;
    other.m_depthTexture = 0;
}

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        
        m_width = other.m_width;
        m_height = other.m_height;
        m_fbo = other.m_fbo;
        m_positionTexture = other.m_positionTexture;
        m_normalTexture = other.m_normalTexture;
        m_albedoTexture = other.m_albedoTexture;
        m_pbrParamsTexture = other.m_pbrParamsTexture;
        m_depthTexture = other.m_depthTexture;
        
        other.m_fbo = 0;
        other.m_positionTexture = 0;
        other.m_normalTexture = 0;
        other.m_albedoTexture = 0;
        other.m_pbrParamsTexture = 0;
        other.m_depthTexture = 0;
    }
    return *this;
}

bool GBuffer::Initialize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        OE_ERROR("GBuffer: Invalid dimensions");
        return false;
    }
    
    m_width = width;
    m_height = height;
    
    // Create FBO
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    CreateTextures();
    AttachTextures();
    
    // Check FBO completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        OE_ERROR_F("GBuffer FBO incomplete: 0x%x", status);
        Destroy();
        return false;
    }
    
    // Set draw buffers for MRT
    GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0, // Position
        GL_COLOR_ATTACHMENT1, // Normal
        GL_COLOR_ATTACHMENT2, // Albedo
        GL_COLOR_ATTACHMENT3  // PBR Params
    };
    glDrawBuffers(4, drawBuffers);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    OE_INFO_F("GBuffer initialized: %ux%u", width, height);
    return true;
}

void GBuffer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) {
        return; // No resize needed
    }
    
    Initialize(width, height);
}

void GBuffer::CreateTextures() {
    // Position (RGB16F)
    glGenTextures(1, &m_positionTexture);
    glBindTexture(GL_TEXTURE_2D, m_positionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Normal (RGB16F)
    glGenTextures(1, &m_normalTexture);
    glBindTexture(GL_TEXTURE_2D, m_normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Albedo (RGB8)
    glGenTextures(1, &m_albedoTexture);
    glBindTexture(GL_TEXTURE_2D, m_albedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // PBR Params (RG8 - Metallic, Roughness)
    glGenTextures(1, &m_pbrParamsTexture);
    glBindTexture(GL_TEXTURE_2D, m_pbrParamsTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, m_width, m_height, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Depth (Depth24)
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GBuffer::AttachTextures() {
    // Attach color textures
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_positionTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normalTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_albedoTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_pbrParamsTexture, 0);
    
    // Attach depth texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
}

void GBuffer::BindForGeometryPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    
    // Clear all attachments
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GBuffer::BindForLightingPass() {
    // Bind textures for reading in lighting pass
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

void GBuffer::BindDepthTexture(GLuint textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
}

void GBuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::Destroy() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    
    if (m_positionTexture != 0) {
        glDeleteTextures(1, &m_positionTexture);
        m_positionTexture = 0;
    }
    
    if (m_normalTexture != 0) {
        glDeleteTextures(1, &m_normalTexture);
        m_normalTexture = 0;
    }
    
    if (m_albedoTexture != 0) {
        glDeleteTextures(1, &m_albedoTexture);
        m_albedoTexture = 0;
    }
    
    if (m_pbrParamsTexture != 0) {
        glDeleteTextures(1, &m_pbrParamsTexture);
        m_pbrParamsTexture = 0;
    }
    
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
}

} // namespace Rendering
} // namespace OpenEye

