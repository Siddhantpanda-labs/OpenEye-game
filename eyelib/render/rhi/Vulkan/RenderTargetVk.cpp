#include "eyelib/render/rhi/Vulkan/RenderTargetVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include "eyelib/render/rhi/Vulkan/TextureVk.h"
#include "eyelib/render/rhi/Vulkan/VkEnums.h"
#include <algorithm>

namespace eyelib {
namespace render {
namespace rhi {

RenderTargetVk* RenderTargetVk::Create(DeviceVk* device, uint32_t width, uint32_t height,
                                        const std::vector<RHITextureFormat>& colorFormats,
                                        RHITextureFormat depthFormat,
                                        uint32_t samples,
                                        const std::vector<TextureVk*>* existingColors) {
    if (!device || width == 0 || height == 0) return nullptr;
    auto* rt = new RenderTargetVk();
    rt->m_device = device;
    rt->m_width = width;
    rt->m_height = height;
    rt->m_ownsTextures = (existingColors == nullptr);
    VkDevice vkDevice = device->GetDevice();
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs;
    VkAttachmentReference depthRef = {};
    depthRef.attachment = VK_ATTACHMENT_UNUSED;
    depthRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    for (size_t i = 0; i < colorFormats.size(); ++i) {
        TextureVk* tex = nullptr;
        if (existingColors && i < existingColors->size()) {
            tex = (*existingColors)[i];
            rt->m_externalColorTextures.push_back(tex);
        } else {
            TextureDesc td;
            td.type = TextureType::Texture2D;
            td.width = width;
            td.height = height;
            td.format = colorFormats[i];
            td.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
            td.samples = samples;
            tex = static_cast<TextureVk*>(device->CreateTexture(td, nullptr));
            if (tex) rt->m_colorTextures.emplace_back(tex);
        }
        if (!tex) continue;
        VkAttachmentDescription att = {};
        att.format = vk::VkEnums::Format(colorFormats[i]);
        att.samples = samples > 1 ? static_cast<VkSampleCountFlagBits>(samples) : VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(att);
        colorRefs.push_back({static_cast<uint32_t>(attachments.size() - 1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    }
    if (depthFormat != RHITextureFormat::None) {
        TextureDesc td;
        td.type = TextureType::Texture2D;
        td.width = width;
        td.height = height;
        td.format = depthFormat;
        td.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
        td.samples = samples;
        rt->m_depthTexture.reset(static_cast<TextureVk*>(device->CreateTexture(td, nullptr)));
        if (rt->m_depthTexture) {
            VkAttachmentDescription att = {};
            att.format = vk::VkEnums::Format(depthFormat);
            att.samples = samples > 1 ? static_cast<VkSampleCountFlagBits>(samples) : VK_SAMPLE_COUNT_1_BIT;
            att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            att.stencilLoadOp = (depthFormat == RHITextureFormat::Depth24Stencil8) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            att.stencilStoreOp = (depthFormat == RHITextureFormat::Depth24Stencil8) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            att.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthRef.attachment = static_cast<uint32_t>(attachments.size());
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(att);
        }
    }
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = (depthRef.attachment != VK_ATTACHMENT_UNUSED) ? &depthRef : nullptr;
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(vkDevice, &rpInfo, nullptr, &rt->m_renderPass) != VK_SUCCESS) {
        Destroy(device, rt);
        return nullptr;
    }
    std::vector<VkImageView> views;
    for (size_t i = 0; i < colorRefs.size(); ++i) {
        TextureVk* tex = rt->m_ownsTextures && i < rt->m_colorTextures.size() ? rt->m_colorTextures[i].get() : (i < rt->m_externalColorTextures.size() ? rt->m_externalColorTextures[i] : nullptr);
        if (tex) views.push_back(tex->m_view);
    }
    if (rt->m_depthTexture) views.push_back(rt->m_depthTexture->m_view);
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = rt->m_renderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(views.size());
    fbInfo.pAttachments = views.data();
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(vkDevice, &fbInfo, nullptr, &rt->m_framebuffer) != VK_SUCCESS) {
        Destroy(device, rt);
        return nullptr;
    }
    return rt;
}

RenderTargetVk* RenderTargetVk::CreateFromTexture(DeviceVk* device, TextureVk* colorTexture, uint32_t face, uint32_t mipLevel) {
    if (!device || !colorTexture || !colorTexture->m_view) return nullptr;
    uint32_t w = std::max(1u, colorTexture->m_width >> mipLevel);
    uint32_t h = std::max(1u, colorTexture->m_height >> mipLevel);
    auto* rt = new RenderTargetVk();
    rt->m_device = device;
    rt->m_width = w;
    rt->m_height = h;
    rt->m_ownsTextures = false;
    rt->m_externalColorTextures.push_back(colorTexture);
    VkAttachmentDescription att = {};
    att.format = vk::VkEnums::Format(colorTexture->m_format);
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &att;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(device->GetDevice(), &rpInfo, nullptr, &rt->m_renderPass) != VK_SUCCESS) { delete rt; return nullptr; }
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = rt->m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &colorTexture->m_view;
    fbInfo.width = w;
    fbInfo.height = h;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(device->GetDevice(), &fbInfo, nullptr, &rt->m_framebuffer) != VK_SUCCESS) {
        vkDestroyRenderPass(device->GetDevice(), rt->m_renderPass, nullptr);
        delete rt;
        return nullptr;
    }
    return rt;
}

void RenderTargetVk::Destroy(DeviceVk* device, RenderTargetVk* rt) {
    if (!device || !rt) return;
    if (rt->m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device->GetDevice(), rt->m_framebuffer, nullptr);
        rt->m_framebuffer = VK_NULL_HANDLE;
    }
    if (rt->m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device->GetDevice(), rt->m_renderPass, nullptr);
        rt->m_renderPass = VK_NULL_HANDLE;
    }
    if (rt->m_ownsTextures) {
        for (auto& t : rt->m_colorTextures) { if (t) { device->DestroyTexture(t.release()); } }
        if (rt->m_depthTexture) { device->DestroyTexture(rt->m_depthTexture.release()); }
    }
    rt->m_colorTextures.clear();
    rt->m_depthTexture.reset();
    rt->m_externalColorTextures.clear();
    delete rt;
}

uint32_t RenderTargetVk::GetWidth() const { return m_width; }
uint32_t RenderTargetVk::GetHeight() const { return m_height; }

ITexture* RenderTargetVk::GetColorTexture(uint32_t index) {
    if (index < m_colorTextures.size()) return m_colorTextures[index].get();
    if (index < m_externalColorTextures.size()) return m_externalColorTextures[index];
    return nullptr;
}

ITexture* RenderTargetVk::GetDepthTexture() {
    return m_depthTexture ? m_depthTexture.get() : m_externalDepthTexture;
}

void* RenderTargetVk::GetNativeFramebuffer() {
    return reinterpret_cast<void*>(m_framebuffer);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
