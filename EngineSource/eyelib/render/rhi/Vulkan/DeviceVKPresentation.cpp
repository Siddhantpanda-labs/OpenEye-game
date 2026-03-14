#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/core/Log.h"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

void DeviceVK::Present() {
    if (!m_immediateCommandList || !m_graphicsQueue) return;
    CommandListVK* imm = m_immediateCommandList.get();
    if (imm->IsRecording())
        imm->End();
    VkDevice device = static_cast<VkDevice>(m_device);
    VkQueue queue = static_cast<VkQueue>(m_graphicsQueue);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (buf != VK_NULL_HANDLE) {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buf;
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    }
    VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
    if (swapchain != VK_NULL_HANDLE && m_currentSwapchainImageIndex != 0xFFFFFFFFu) {
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &m_currentSwapchainImageIndex;
        PFN_vkQueuePresentKHR pfnPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
        if (pfnPresent) pfnPresent(queue, &presentInfo);
        m_currentSwapchainImageIndex = 0xFFFFFFFFu;
    }
    vkQueueWaitIdle(queue);
}

void DeviceVK::OnWindowResized(uint32_t width, uint32_t height) {
    width = std::max<uint32_t>(1u, width);
    height = std::max<uint32_t>(1u, height);
    if (width == m_swapchainWidth && height == m_swapchainHeight)
        return;
    if (!m_device || !m_surface)
        return;

    VkDevice device = static_cast<VkDevice>(m_device);
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(m_physicalDevice);
    VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(m_surface);
    vkDeviceWaitIdle(device);

    for (void* fb : m_swapchainFramebuffers) {
        if (fb) vkDestroyFramebuffer(device, static_cast<VkFramebuffer>(fb), nullptr);
    }
    m_swapchainFramebuffers.clear();
    for (void* view : m_swapchainImageViews) {
        if (view) vkDestroyImageView(device, static_cast<VkImageView>(view), nullptr);
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    if (m_swapchain != nullptr) {
        vkDestroySwapchainKHR(device, static_cast<VkSwapchainKHR>(m_swapchain), nullptr);
        m_swapchain = nullptr;
    }

    VkSurfaceCapabilitiesKHR caps = {};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::OnWindowResized: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (0x%x)", res);
        return;
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());
    VkSurfaceFormatKHR chosenFormat = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR pm : presentModes) {
        if (pm == VK_PRESENT_MODE_FIFO_KHR) { presentMode = pm; break; }
    }

    VkExtent2D extent = {};
    extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swapInfo = {};
    swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface = surface;
    swapInfo.minImageCount = imageCount;
    swapInfo.imageFormat = chosenFormat.format;
    swapInfo.imageColorSpace = chosenFormat.colorSpace;
    swapInfo.imageExtent = extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = caps.currentTransform;
    swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode = presentMode;
    swapInfo.clipped = VK_TRUE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    res = vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::OnWindowResized: vkCreateSwapchainKHR failed (0x%x)", res);
        return;
    }

    m_swapchain = swapchain;
    m_swapchainWidth = extent.width;
    m_swapchainHeight = extent.height;
    const uint32_t oldFormat = m_swapchainFormat;
    m_swapchainFormat = static_cast<uint32_t>(chosenFormat.format);
    m_currentSwapchainImageIndex = 0xFFFFFFFFu;

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
    m_swapchainImages.reserve(swapchainImageCount);
    for (VkImage img : swapchainImages) m_swapchainImages.push_back(img);
    m_swapchainImageViews.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView = VK_NULL_HANDLE;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) == VK_SUCCESS)
            m_swapchainImageViews[i] = imageView;
    }

    if (oldFormat != m_swapchainFormat && m_swapchainRenderPass != nullptr) {
        vkDestroyRenderPass(device, static_cast<VkRenderPass>(m_swapchainRenderPass), nullptr);
        m_swapchainRenderPass = nullptr;
    }
    EnsureSwapchainRenderPassAndFramebuffers();
}

void DeviceVK::Finish() {
    if (m_graphicsQueue)
        vkQueueWaitIdle(static_cast<VkQueue>(m_graphicsQueue));
}

void DeviceVK::EnsureSwapchainRenderPassAndFramebuffers() {
    if (m_swapchainRenderPass != nullptr && !m_swapchainFramebuffers.empty()) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device == VK_NULL_HANDLE || m_swapchainImageViews.empty()) return;
    VkFormat fmt = static_cast<VkFormat>(m_swapchainFormat);
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkAttachmentDescription att = {};
    att.format = fmt;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &att;
    VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &rp) != VK_SUCCESS) return;
    m_swapchainRenderPass = rp;
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        VkImageView view = static_cast<VkImageView>(m_swapchainImageViews[i]);
        if (view == VK_NULL_HANDLE) continue;
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = rp;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &view;
        fbInfo.width = m_swapchainWidth;
        fbInfo.height = m_swapchainHeight;
        fbInfo.layers = 1;
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &fb) == VK_SUCCESS)
            m_swapchainFramebuffers[i] = fb;
    }
}

void DeviceVK::EnsureImGuiDescriptorPool() {
    if (m_imGuiDescriptorPool != nullptr) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device == VK_NULL_HANDLE) return;
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS)
        m_imGuiDescriptorPool = pool;
}

void DeviceVK::SetPostBlitCallback(std::function<void(void*)> cb) {
    m_postBlitCallback = std::move(cb);
}

bool DeviceVK::GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) {
    if (!out || !m_device || !m_swapchain) return false;
    EnsureImGuiDescriptorPool();
    EnsureSwapchainRenderPassAndFramebuffers();
    if (m_imGuiDescriptorPool == nullptr || m_swapchainRenderPass == nullptr) return false;
    out->instance = m_instance;
    out->physical_device = m_physicalDevice;
    out->device = m_device;
    out->queue_family = m_graphicsQueueFamily;
    out->queue = m_graphicsQueue;
    out->pipeline_cache = m_pipelineCache;
    out->descriptor_pool = m_imGuiDescriptorPool;
    out->min_image_count = 2;
    out->image_count = static_cast<uint32_t>(m_swapchainImages.size());
    out->render_pass = m_swapchainRenderPass;
    return true;
}

void DeviceVK::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!m_swapchain || m_swapchainImages.empty() || !m_immediateCommandList) return;
    if (!src) {
        if (!m_postBlitCallback || !m_swapchainRenderPass || m_swapchainFramebuffers.empty()) return;
        VkDevice device = static_cast<VkDevice>(m_device);
        VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
        VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &m_currentSwapchainImageIndex);
        if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;
        if (m_currentSwapchainImageIndex >= m_swapchainFramebuffers.size() || !m_swapchainFramebuffers[m_currentSwapchainImageIndex]) return;
        CommandListVK* imm = m_immediateCommandList.get();
        imm->EndRenderingIfActive();
        if (!imm->IsRecording())
            imm->Begin();
        VkCommandBuffer cmd = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
        if (!imm->IsRecording() || cmd == VK_NULL_HANDLE) return;
        VkImage dstImage = static_cast<VkImage>(m_swapchainImages[m_currentSwapchainImageIndex]);
        VkImageMemoryBarrier barrierDst = {};
        barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierDst.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrierDst.image = dstImage;
        barrierDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
        VkRenderPass rp = static_cast<VkRenderPass>(m_swapchainRenderPass);
        VkFramebuffer fb = static_cast<VkFramebuffer>(m_swapchainFramebuffers[m_currentSwapchainImageIndex]);
        VkClearValue clearVal = {};
        VkRenderPassBeginInfo rpBegin = {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = rp;
        rpBegin.framebuffer = fb;
        rpBegin.renderArea = { { 0, 0 }, { m_swapchainWidth, m_swapchainHeight } };
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearVal;
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        m_postBlitCallback(cmd);
        vkCmdEndRenderPass(cmd);
        return;
    }
    RenderTargetVK* rt = static_cast<RenderTargetVK*>(src);
    ITexture* color0 = rt->GetColorTexture(0);
    if (!color0) return;
    TextureVK* tex = static_cast<TextureVK*>(color0);
    VkImage srcImage = static_cast<VkImage>(tex->m_image);
    if (srcImage == VK_NULL_HANDLE) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &m_currentSwapchainImageIndex);
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;
    if (m_currentSwapchainImageIndex >= m_swapchainImages.size()) return;
    CommandListVK* imm = m_immediateCommandList.get();
    imm->EndRenderingIfActive();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (!imm->IsRecording() || cmd == VK_NULL_HANDLE) return;
    VkImage dstImage = static_cast<VkImage>(m_swapchainImages[m_currentSwapchainImageIndex]);
    uint32_t srcW = tex->m_width;
    uint32_t srcH = tex->m_height;
    uint32_t w = std::min(srcW, m_swapchainWidth);
    uint32_t h = std::min(srcH, m_swapchainHeight);
    if (w == 0 || h == 0) return;
    VkImageMemoryBarrier barrierSrc = {};
    barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierSrc.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrierSrc.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierSrc.image = srcImage;
    barrierSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierSrc);
    VkImageMemoryBarrier barrierDst = {};
    barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierDst.image = dstImage;
    barrierDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
    VkImageCopy copy = {};
    copy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.extent = { w, h, 1 };
    vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    if (m_postBlitCallback && m_swapchainRenderPass != nullptr && m_currentSwapchainImageIndex < m_swapchainFramebuffers.size() && m_swapchainFramebuffers[m_currentSwapchainImageIndex] != nullptr) {
        barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
        VkRenderPass rp = static_cast<VkRenderPass>(m_swapchainRenderPass);
        VkFramebuffer fb = static_cast<VkFramebuffer>(m_swapchainFramebuffers[m_currentSwapchainImageIndex]);
        VkClearValue clearVal = {};
        VkRenderPassBeginInfo rpBegin = {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = rp;
        rpBegin.framebuffer = fb;
        rpBegin.renderArea = { { 0, 0 }, { m_swapchainWidth, m_swapchainHeight } };
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearVal;
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        m_postBlitCallback(cmd);
        vkCmdEndRenderPass(cmd);
        /* Image already in PRESENT_SRC_KHR via render pass finalLayout */
    } else {
        barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.dstAccessMask = 0;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
    }
}

void DeviceVK::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) { (void)nativeFbo; (void)width; (void)height; }

void DeviceVK::BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) { (void)src; (void)dstX; (void)dstY; (void)dstW; (void)dstH; }

void DeviceVK::BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) {
    (void)windowWidth;
    (void)windowHeight;
    BlitToDefaultFramebuffer(src);
}

void DeviceVK::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) { (void)src; (void)dst; (void)colorOnly; }

void DeviceVK::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) { (void)nativeFbo; (void)dst; (void)colorOnly; }

void DeviceVK::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) { (void)src; (void)dst; }

bool DeviceVK::ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) { (void)rt; (void)x; (void)y; (void)outValue; return false; }

bool DeviceVK::ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) { (void)rt; (void)x; (void)y; (void)w; (void)h; (void)outBuffer; return false; }

void* DeviceVK::CreatePixelPackBuffer(size_t sizeInBytes) { (void)sizeInBytes; return nullptr; }

void DeviceVK::DestroyPixelPackBuffer(void* handle) { (void)handle; }

void DeviceVK::ReadPixelsToPBO(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* pboHandle) { (void)rt; (void)x; (void)y; (void)w; (void)h; (void)pboHandle; }

void DeviceVK::MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; (void)width; (void)height; }

void DeviceVK::MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; }

} // namespace rhi
} // namespace render
} // namespace eyelib
