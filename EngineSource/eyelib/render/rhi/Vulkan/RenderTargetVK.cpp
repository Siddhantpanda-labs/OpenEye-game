#include "eyelib/render/rhi/Vulkan/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"

namespace eyelib {
namespace render {
namespace rhi {

RenderTargetVK::RenderTargetVK(DeviceVK* device) : m_device(device) {}

RenderTargetVK::~RenderTargetVK() = default;

uint32_t RenderTargetVK::GetWidth() const { return m_width; }

uint32_t RenderTargetVK::GetHeight() const { return m_height; }

ITexture* RenderTargetVK::GetColorTexture(uint32_t index) {
    if (index < m_colorTextures.size()) return m_colorTextures[index].get();
    if (index < m_externalColorTextures.size()) return m_externalColorTextures[index];
    return nullptr;
}

ITexture* RenderTargetVK::GetDepthTexture() {
    if (m_depthTexture) return m_depthTexture.get();
    return m_externalDepthTexture;
}

void* RenderTargetVK::GetNativeFramebuffer() { return m_framebuffer; }

void RenderTargetVK::SetDepthLayer(uint32_t layer) { m_depthArrayLayer = layer; }

uint32_t RenderTargetVK::GetDepthLayer() const { return m_depthArrayLayer; }

void* RenderTargetVK::GetColorImageView(uint32_t index) const {
    if (index < m_colorAttachmentViews.size() && m_colorAttachmentViews[index] != nullptr)
        return m_colorAttachmentViews[index];
    if (index < m_colorTextures.size() && m_colorTextures[index])
        return m_colorTextures[index]->m_imageView;
    if (index < m_externalColorTextures.size() && m_externalColorTextures[index])
        return m_externalColorTextures[index]->m_imageView;
    return nullptr;
}

void* RenderTargetVK::GetDepthImageView() const {
    if (!m_depthLayerViews.empty() && m_depthArrayLayer < m_depthLayerViews.size() && m_depthLayerViews[m_depthArrayLayer] != nullptr)
        return m_depthLayerViews[m_depthArrayLayer];
    if (m_depthAttachmentView != nullptr)
        return m_depthAttachmentView;
    if (m_depthTexture) return m_depthTexture->m_imageView;
    if (m_externalDepthTexture) return m_externalDepthTexture->m_imageView;
    return nullptr;
}

uint32_t RenderTargetVK::GetColorAttachmentCount() const {
    uint32_t n = static_cast<uint32_t>(m_colorTextures.size());
    if (n > 0) return n;
    return static_cast<uint32_t>(m_externalColorTextures.size());
}

} // namespace rhi
} // namespace render
} // namespace eyelib
