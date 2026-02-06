#include "eyelib/render/RenderTarget.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
static GLenum ToGLInternalFormat(TextureFormat f, bool isSRGB) {
    switch (f) {
        case TextureFormat::R8: return GL_R8;
        case TextureFormat::RG8: return GL_RG8;
        case TextureFormat::RGB8: return isSRGB ? GL_SRGB8 : GL_RGB8;
        case TextureFormat::RGBA8: return isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        case TextureFormat::RGB16F: return GL_RGB16F;
        case TextureFormat::RGBA16F: return GL_RGBA16F;
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        case TextureFormat::None: return GL_RGBA8;
        default: return GL_RGBA8;
    }
}
}

RenderTarget::RenderTarget() = default;

RenderTarget::~RenderTarget() {
    if (m_deviceRHI) Destroy(m_deviceRHI);
    Destroy();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height), m_samples(other.m_samples),
      m_fbo(other.m_fbo), m_resolveFBO(other.m_resolveFBO),
      m_colorFormats(std::move(other.m_colorFormats)),
      m_depthFormat(other.m_depthFormat), m_deviceRHI(other.m_deviceRHI),
      m_renderTargetRHI(other.m_renderTargetRHI),
      m_colorTextures(std::move(other.m_colorTextures)),
      m_depthTexture(std::move(other.m_depthTexture)),
      m_colorRenderbuffers(std::move(other.m_colorRenderbuffers)),
      m_depthRenderbuffer(other.m_depthRenderbuffer) {
    other.m_fbo = 0;
    other.m_resolveFBO = 0;
    other.m_depthRenderbuffer = 0;
    other.m_deviceRHI = nullptr;
    other.m_renderTargetRHI = nullptr;
    other.m_width = other.m_height = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        if (m_deviceRHI) Destroy(m_deviceRHI);
        Destroy();
        m_width = other.m_width;
        m_height = other.m_height;
        m_samples = other.m_samples;
        m_fbo = other.m_fbo;
        m_resolveFBO = other.m_resolveFBO;
        m_colorFormats = std::move(other.m_colorFormats);
        m_depthFormat = other.m_depthFormat;
        m_deviceRHI = other.m_deviceRHI;
        m_renderTargetRHI = other.m_renderTargetRHI;
        m_colorTextures = std::move(other.m_colorTextures);
        m_depthTexture = std::move(other.m_depthTexture);
        m_colorRenderbuffers = std::move(other.m_colorRenderbuffers);
        m_depthRenderbuffer = other.m_depthRenderbuffer;
        other.m_fbo = 0;
        other.m_resolveFBO = 0;
        other.m_depthRenderbuffer = 0;
        other.m_deviceRHI = nullptr;
        other.m_renderTargetRHI = nullptr;
        other.m_width = other.m_height = 0;
    }
    return *this;
}

bool RenderTarget::Create(uint32_t width, uint32_t height,
                          const std::vector<TextureFormat>& colorFormats,
                          TextureFormat depthFormat, int samples) {
    Destroy();
    if (width == 0 || height == 0) return false;
    m_width = width;
    m_height = height;
    m_samples = GetMaxSamples(samples);  // Clamp to supported MSAA
    m_colorFormats = colorFormats;
    m_depthFormat = depthFormat;

    // Always create resolved textures (for reading)
    CreateTextures();

    if (m_samples > 0) {
        // MSAA: create multisampled renderbuffers and resolve FBO
        CreateMSAABuffers();
        CreateResolveFBO();
    } else {
        // Non-MSAA: attach textures directly to main FBO
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        AttachTextures();
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        core::Log::ErrorF("RenderTarget: Framebuffer incomplete: 0x%x", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        Destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void RenderTarget::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    Create(width, height, m_colorFormats, m_depthFormat, m_samples);
}

bool RenderTarget::Create(rhi::IDevice* device, uint32_t width, uint32_t height,
                          const std::vector<TextureFormat>& colorFormats,
                          TextureFormat depthFormat, uint32_t samples) {
    if (!device || width == 0 || height == 0) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    std::vector<rhi::RHITextureFormat> rhiColor;
    for (TextureFormat f : colorFormats)
        rhiColor.push_back(Texture2D::TextureFormatToRHI(f));
    rhi::RHITextureFormat rhiDepth = Texture2D::TextureFormatToRHI(depthFormat);
    if (depthFormat == TextureFormat::None) rhiDepth = rhi::RHITextureFormat::None;
    m_renderTargetRHI = device->CreateRenderTarget(width, height, rhiColor, rhiDepth, samples);
    if (!m_renderTargetRHI) return false;
    m_width = width;
    m_height = height;
    m_samples = static_cast<int>(samples);
    m_deviceRHI = device;
    m_colorFormats = colorFormats;
    m_depthFormat = depthFormat;
    return true;
}

void RenderTarget::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_renderTargetRHI) { m_deviceRHI->DestroyRenderTarget(m_renderTargetRHI); m_renderTargetRHI = nullptr; }
    m_deviceRHI = nullptr;
    m_width = m_height = 0;
}

void RenderTarget::Destroy() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_resolveFBO) {
        glDeleteFramebuffers(1, &m_resolveFBO);
        m_resolveFBO = 0;
    }
    if (!m_colorRenderbuffers.empty()) {
        glDeleteRenderbuffers(static_cast<GLsizei>(m_colorRenderbuffers.size()), m_colorRenderbuffers.data());
        m_colorRenderbuffers.clear();
    }
    if (m_depthRenderbuffer) {
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
        m_depthRenderbuffer = 0;
    }
    m_colorTextures.clear();
    m_depthTexture.reset();
    m_width = m_height = 0;
}

void RenderTarget::Bind() const {
    if (m_fbo) glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void RenderTarget::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::Resolve() {
    if (m_samples == 0 || !m_resolveFBO || !m_fbo) return;
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolveFBO);
    
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (m_depthTexture && m_depthTexture->IsValid()) {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    
    glBlitFramebuffer(0, 0, m_width, m_height,
                      0, 0, m_width, m_height,
                      mask, GL_LINEAR);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::CreateTextures() {
    m_colorTextures.clear();
    for (TextureFormat fmt : m_colorFormats) {
        auto tex = std::make_shared<Texture2D>();
        if (!tex->Create(static_cast<int>(m_width), static_cast<int>(m_height), fmt, nullptr)) {
            core::Log::ErrorF("RenderTarget: Failed to create color texture");
            continue;
        }
        tex->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
        tex->SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
        m_colorTextures.push_back(tex);
    }

    if (m_depthFormat == TextureFormat::Depth24 || m_depthFormat == TextureFormat::Depth24Stencil8) {
        m_depthTexture = std::make_shared<Texture2D>();
        if (!m_depthTexture->Create(static_cast<int>(m_width), static_cast<int>(m_height), m_depthFormat, nullptr)) {
            core::Log::ErrorF("RenderTarget: Failed to create depth texture");
            m_depthTexture.reset();
        }
    }
}

void RenderTarget::CreateMSAABuffers() {
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create multisampled color renderbuffers
    m_colorRenderbuffers.resize(m_colorFormats.size());
    if (!m_colorRenderbuffers.empty()) {
        glGenRenderbuffers(static_cast<GLsizei>(m_colorRenderbuffers.size()), m_colorRenderbuffers.data());
    }
    
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < m_colorFormats.size(); ++i) {
        GLenum internalFormat = ToGLInternalFormat(m_colorFormats[i], false);
        glBindRenderbuffer(GL_RENDERBUFFER, m_colorRenderbuffers[i]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, internalFormat, m_width, m_height);
        GLenum attachment = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_colorRenderbuffers[i]);
        drawBuffers.push_back(attachment);
    }
    
    if (!drawBuffers.empty()) {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    } else {
        glDrawBuffer(GL_NONE);
    }

    // Create multisampled depth renderbuffer
    if (m_depthFormat == TextureFormat::Depth24 || m_depthFormat == TextureFormat::Depth24Stencil8) {
        glGenRenderbuffers(1, &m_depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
        GLenum depthFormat = (m_depthFormat == TextureFormat::Depth24Stencil8) ?
            GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, depthFormat, m_width, m_height);
        GLenum attachment = (m_depthFormat == TextureFormat::Depth24Stencil8) ?
            GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_depthRenderbuffer);
    }
    
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void RenderTarget::CreateResolveFBO() {
    glGenFramebuffers(1, &m_resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_resolveFBO);
    AttachTextures();
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        core::Log::ErrorF("RenderTarget: Resolve FBO incomplete: 0x%x", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::AttachTextures() {
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < m_colorTextures.size(); ++i) {
        if (!m_colorTextures[i] || !m_colorTextures[i]->IsValid()) continue;
        GLenum attachment = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, m_colorTextures[i]->Id(), 0);
        drawBuffers.push_back(attachment);
    }
    if (!drawBuffers.empty()) {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    } else {
        glDrawBuffer(GL_NONE);
    }

    if (m_depthTexture && m_depthTexture->IsValid()) {
        GLenum attachment = (m_depthFormat == TextureFormat::Depth24Stencil8) ?
            GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, m_depthTexture->Id(), 0);
    }
}

void RenderTarget::Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly) {
    if (!source.m_fbo || !dest.m_fbo) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest.m_fbo);
    GLbitfield mask = colorOnly ? GL_COLOR_BUFFER_BIT : (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlitFramebuffer(0, 0, source.m_width, source.m_height,
                      0, 0, dest.m_width, dest.m_height,
                      mask, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::shared_ptr<Texture2D> RenderTarget::GetColorTexture(size_t slot) const {
    if (slot >= m_colorTextures.size()) return nullptr;
    return m_colorTextures[slot];
}

int RenderTarget::GetMaxSamples(int samples) {
    if (samples <= 0) return 0;
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    return std::min(samples, static_cast<int>(maxSamples));
}

} // namespace render
} // namespace eyelib
