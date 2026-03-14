#include "eyelib/render/rhi/Vulkan/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>

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

IRenderTarget* CreateRenderTargetVK(DeviceVK* device, uint32_t width, uint32_t height,
                                    const std::vector<RHITextureFormat>& colorFormats,
                                    RHITextureFormat depthFormat, uint32_t samples, bool transient) {
    if (!device || width == 0 || height == 0) return nullptr;
    auto* rt = new RenderTargetVK(device);
    rt->m_width = width;
    rt->m_height = height;
    TextureDesc texDesc;
    texDesc.width = width;
    texDesc.height = height;
    texDesc.mipLevels = 1;
    texDesc.samples = (samples > 0) ? samples : 1u;
    texDesc.usage = transient ? TextureUsage::RenderTarget : (TextureUsage::ShaderResource | TextureUsage::RenderTarget);
    texDesc.transient = transient;
    for (RHITextureFormat fmt : colorFormats) {
        texDesc.format = fmt;
        ITexture* tex = device->CreateTexture(texDesc, nullptr);
        if (!tex) { DestroyRenderTargetVK(device, rt); return nullptr; }
        rt->m_colorTextures.push_back(std::unique_ptr<TextureVK>(static_cast<TextureVK*>(tex)));
    }
    if (depthFormat != RHITextureFormat::None) {
        texDesc.format = depthFormat;
        texDesc.usage = transient ? TextureUsage::RenderTarget : (TextureUsage::ShaderResource | TextureUsage::RenderTarget);
        texDesc.transient = transient;
        ITexture* depthTex = device->CreateTexture(texDesc, nullptr);
        if (!depthTex) { DestroyRenderTargetVK(device, rt); return nullptr; }
        rt->m_depthTexture = std::unique_ptr<TextureVK>(static_cast<TextureVK*>(depthTex));
    }
    return rt;
}

IRenderTarget* CreateDepthOnlyRenderTargetArrayVK(DeviceVK* deviceObj, uint32_t width, uint32_t height, uint32_t arrayLayers) {
    if (!deviceObj || width == 0 || height == 0 || arrayLayers == 0) return nullptr;
    auto* rt = new RenderTargetVK(deviceObj);
    rt->m_width = width;
    rt->m_height = height;

    TextureDesc td;
    td.type = TextureType::Texture2DArray;
    td.width = width;
    td.height = height;
    td.depthOrArrayLayers = arrayLayers;
    td.format = RHITextureFormat::Depth24;
    td.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
    td.mipLevels = 1;
    td.samples = 1;

    ITexture* depth = deviceObj->CreateTexture(td, nullptr);
    if (!depth) { delete rt; return nullptr; }
    rt->m_depthTexture = std::unique_ptr<TextureVK>(static_cast<TextureVK*>(depth));
    rt->m_depthLayerViews.resize(arrayLayers, nullptr);

    TextureVK* depthTex = rt->m_depthTexture.get();
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkImage image = static_cast<VkImage>(depthTex->m_image);
    VkFormat format = ToVkFormat(depthTex->m_format);
    for (uint32_t layer = 0; layer < arrayLayers; ++layer) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = ToVkImageAspect(depthTex->m_format);
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = layer;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView layerView = VK_NULL_HANDLE;
        if (vkCreateImageView(device, &viewInfo, nullptr, &layerView) != VK_SUCCESS) {
            DestroyRenderTargetVK(deviceObj, rt);
            return nullptr;
        }
        rt->m_depthLayerViews[layer] = layerView;
    }
    return rt;
}

IRenderTarget* CreateRenderTargetWithSharedDepthVK(DeviceVK* deviceObj, uint32_t width, uint32_t height,
                                                   const std::vector<RHITextureFormat>& colorFormats,
                                                   ITexture* sharedDepthTexture, uint32_t samples) {
    if (!deviceObj || width == 0 || height == 0 || colorFormats.empty() || !sharedDepthTexture) return nullptr;
    auto* sharedDepth = static_cast<TextureVK*>(sharedDepthTexture);
    if (!sharedDepth->m_image || !sharedDepth->m_imageView) return nullptr;
    auto* rt = new RenderTargetVK(deviceObj);
    rt->m_width = width;
    rt->m_height = height;
    rt->m_externalDepthTexture = sharedDepth;

    TextureDesc texDesc;
    texDesc.type = TextureType::Texture2D;
    texDesc.width = width;
    texDesc.height = height;
    texDesc.mipLevels = 1;
    texDesc.samples = (samples > 0) ? samples : 1u;
    texDesc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
    for (RHITextureFormat fmt : colorFormats) {
        texDesc.format = fmt;
        ITexture* tex = deviceObj->CreateTexture(texDesc, nullptr);
        if (!tex) {
            DestroyRenderTargetVK(deviceObj, rt);
            return nullptr;
        }
        rt->m_colorTextures.push_back(std::unique_ptr<TextureVK>(static_cast<TextureVK*>(tex)));
    }
    return rt;
}

IRenderTarget* CreateRenderTargetFromTextureVK(DeviceVK* deviceObj, ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    if (!deviceObj) return nullptr;
    auto* color = static_cast<TextureVK*>(colorTexture);
    if (!color || !color->m_image) return nullptr;
    auto* rt = new RenderTargetVK(deviceObj);
    uint32_t mipWidth = std::max<uint32_t>(1u, color->m_width >> mipLevel);
    uint32_t mipHeight = std::max<uint32_t>(1u, color->m_height >> mipLevel);
    rt->m_width = mipWidth;
    rt->m_height = mipHeight;
    rt->m_ownsTextures = false;
    rt->m_externalColorTextures.push_back(color);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = static_cast<VkImage>(color->m_image);
    viewInfo.format = ToVkFormat(color->m_format);
    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(color->m_format);
    viewInfo.subresourceRange.baseMipLevel = mipLevel;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (color->m_type == TextureType::TextureCube || color->m_type == TextureType::Texture2DArray) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.baseArrayLayer = face;
    } else if (color->m_type == TextureType::Texture2D) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    } else {
        delete rt;
        return nullptr;
    }

    VkImageView colorView = VK_NULL_HANDLE;
    if (vkCreateImageView(static_cast<VkDevice>(deviceObj->GetDevice()), &viewInfo, nullptr, &colorView) != VK_SUCCESS) {
        delete rt;
        return nullptr;
    }
    rt->m_colorAttachmentViews.push_back(colorView);
    return rt;
}

IRenderTarget* CreateRenderTargetFromTextureWithDepthVK(DeviceVK* deviceObj, ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) {
    if (!deviceObj) return nullptr;
    auto* color = static_cast<TextureVK*>(colorTexture);
    auto* depth = static_cast<TextureVK*>(depthTexture);
    if (!color || !depth || !color->m_image || !depth->m_image) return nullptr;
    IRenderTarget* baseRt = CreateRenderTargetFromTextureVK(deviceObj, colorTexture, face, mipLevel);
    if (!baseRt) return nullptr;
    auto* rt = static_cast<RenderTargetVK*>(baseRt);
    rt->m_externalDepthTexture = depth;
    return rt;
}

void DestroyRenderTargetVK(DeviceVK* deviceObj, IRenderTarget* rt) {
    if (!deviceObj || !rt) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (device != VK_NULL_HANDLE) {
        for (void* v : rtVK->m_colorAttachmentViews) {
            if (v) vkDestroyImageView(device, static_cast<VkImageView>(v), nullptr);
        }
        rtVK->m_colorAttachmentViews.clear();
        if (rtVK->m_depthAttachmentView) {
            vkDestroyImageView(device, static_cast<VkImageView>(rtVK->m_depthAttachmentView), nullptr);
            rtVK->m_depthAttachmentView = nullptr;
        }
        for (void* v : rtVK->m_depthLayerViews) {
            if (v) vkDestroyImageView(device, static_cast<VkImageView>(v), nullptr);
        }
        rtVK->m_depthLayerViews.clear();
    }
    for (auto& t : rtVK->m_colorTextures) {
        deviceObj->DestroyTexture(t.release());
    }
    rtVK->m_colorTextures.clear();
    if (rtVK->m_depthTexture) {
        deviceObj->DestroyTexture(rtVK->m_depthTexture.release());
    }
    rtVK->m_externalColorTextures.clear();
    rtVK->m_externalDepthTexture = nullptr;
    delete rtVK;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
