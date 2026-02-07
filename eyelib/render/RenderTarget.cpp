#include "eyelib/render/RenderTarget.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <algorithm>

namespace eyelib {
namespace render {

RenderTarget::RenderTarget() = default;

RenderTarget::~RenderTarget() {
    if (m_deviceRHI && m_renderTargetRHI) {
        m_deviceRHI->DestroyRenderTarget(m_renderTargetRHI);
        m_renderTargetRHI = nullptr;
        m_deviceRHI = nullptr;
    }
    m_colorTextures.clear();
    m_depthTexture.reset();
    m_width = m_height = 0;
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height), m_samples(other.m_samples),
      m_colorFormats(std::move(other.m_colorFormats)),
      m_depthFormat(other.m_depthFormat), m_deviceRHI(other.m_deviceRHI),
      m_renderTargetRHI(other.m_renderTargetRHI),
      m_colorTextures(std::move(other.m_colorTextures)),
      m_depthTexture(std::move(other.m_depthTexture)) {
    other.m_renderTargetRHI = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_width = other.m_height = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        if (m_deviceRHI) Destroy(m_deviceRHI);
        m_width = other.m_width;
        m_height = other.m_height;
        m_samples = other.m_samples;
        m_colorFormats = std::move(other.m_colorFormats);
        m_depthFormat = other.m_depthFormat;
        m_deviceRHI = other.m_deviceRHI;
        m_renderTargetRHI = other.m_renderTargetRHI;
        m_colorTextures = std::move(other.m_colorTextures);
        m_depthTexture = std::move(other.m_depthTexture);
        other.m_renderTargetRHI = nullptr;
        other.m_deviceRHI = nullptr;
        other.m_width = other.m_height = 0;
    }
    return *this;
}

bool RenderTarget::Create(rhi::IDevice* device, uint32_t width, uint32_t height,
                          const std::vector<TextureFormat>& colorFormats,
                          TextureFormat depthFormat, uint32_t samples) {
    if (!device || width == 0 || height == 0) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    uint32_t clampedSamples = (samples <= 1) ? 1u : static_cast<uint32_t>(GetMaxSamples(device, static_cast<int>(samples)));
    std::vector<rhi::RHITextureFormat> rhiColor;
    for (TextureFormat f : colorFormats)
        rhiColor.push_back(Texture2D::TextureFormatToRHI(f));
    rhi::RHITextureFormat rhiDepth = (depthFormat == TextureFormat::None) ? rhi::RHITextureFormat::None : Texture2D::TextureFormatToRHI(depthFormat);
    m_renderTargetRHI = device->CreateRenderTarget(width, height, rhiColor, rhiDepth, clampedSamples);
    if (!m_renderTargetRHI) return false;
    m_width = width;
    m_height = height;
    m_samples = static_cast<int>(clampedSamples);
    m_deviceRHI = device;
    m_colorFormats = colorFormats;
    m_depthFormat = depthFormat;
    CacheTextureWrappers();
    return true;
}

void RenderTarget::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_renderTargetRHI) {
        m_deviceRHI->DestroyRenderTarget(m_renderTargetRHI);
        m_renderTargetRHI = nullptr;
    }
    m_deviceRHI = nullptr;
    m_colorTextures.clear();
    m_depthTexture.reset();
    m_width = m_height = 0;
}

void RenderTarget::Destroy() {
    m_renderTargetRHI = nullptr;
    m_deviceRHI = nullptr;
    m_colorTextures.clear();
    m_depthTexture.reset();
    m_width = m_height = 0;
}

void RenderTarget::Bind() const {
    (void)this;
}

void RenderTarget::Unbind() const {
    (void)this;
}

void RenderTarget::Resolve() {
}

void RenderTarget::CacheTextureWrappers() {
    if (!m_renderTargetRHI) return;
    m_colorTextures.clear();
    for (size_t i = 0; i < m_colorFormats.size(); ++i) {
        rhi::ITexture* tex = m_renderTargetRHI->GetColorTexture(static_cast<uint32_t>(i));
        if (!tex) break;
        auto wrapper = std::make_shared<Texture2D>();
        wrapper->WrapRHI(tex);
        m_colorTextures.push_back(wrapper);
    }
    rhi::ITexture* depthTex = m_renderTargetRHI->GetDepthTexture();
    if (depthTex) {
        m_depthTexture = std::make_shared<Texture2D>();
        m_depthTexture->WrapRHI(depthTex);
    } else {
        m_depthTexture.reset();
    }
}

void RenderTarget::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    if (!m_deviceRHI || m_colorFormats.empty()) return;
    Create(m_deviceRHI, width, height, m_colorFormats, m_depthFormat, static_cast<uint32_t>(m_samples));
}

void RenderTarget::Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly) {
    if (!source.m_renderTargetRHI || !dest.m_renderTargetRHI) return;
    if (source.m_deviceRHI != dest.m_deviceRHI) return;
    source.m_deviceRHI->Blit(source.m_renderTargetRHI, dest.m_renderTargetRHI, colorOnly);
}

unsigned int RenderTarget::GetFBO() const {
    if (!m_renderTargetRHI) return 0;
    void* native = m_renderTargetRHI->GetNativeFramebuffer();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

std::shared_ptr<Texture2D> RenderTarget::GetColorTexture(size_t slot) const {
    if (slot >= m_colorTextures.size()) return nullptr;
    return m_colorTextures[slot];
}

int RenderTarget::GetMaxSamples(rhi::IDevice* device, int samples) {
    if (!device || samples <= 0) return 0;
    return device->GetMaxSamples(samples);
}

} // namespace render
} // namespace eyelib
