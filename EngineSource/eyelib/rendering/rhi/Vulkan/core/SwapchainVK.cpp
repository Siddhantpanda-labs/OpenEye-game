#include "eyelib/rendering/rhi/Vulkan/core/SwapchainVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Create one binary semaphore per swapchain image for render-finished (submit signals, present waits). */
static bool CreateRenderFinishedSemaphores(SwapchainVK* sw) {
    if (!sw || !sw->device || sw->images.empty()) return false;
    VkDevice dev = static_cast<VkDevice>(sw->device);
    sw->renderFinishedSemaphores.resize(sw->images.size(), nullptr);
    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (size_t i = 0; i < sw->images.size(); ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        if (vkCreateSemaphore(dev, &semInfo, nullptr, &sem) != VK_SUCCESS)
            return false;
        sw->renderFinishedSemaphores[i] = sem;
    }
    return true;
}

static void DestroyRenderFinishedSemaphores(SwapchainVK* sw) {
    if (!sw || !sw->device) return;
    VkDevice dev = static_cast<VkDevice>(sw->device);
    for (void* p : sw->renderFinishedSemaphores) {
        if (p) vkDestroySemaphore(dev, static_cast<VkSemaphore>(p), nullptr);
    }
    sw->renderFinishedSemaphores.clear();
}

static void EnsureRenderPassAndFramebuffers(SwapchainVK* sw) {
    if (!sw || !sw->device || sw->imageViews.empty()) return;
    if (sw->renderPass != nullptr && !sw->framebuffers.empty()) return;
    VkDevice device = static_cast<VkDevice>(sw->device);
    VkFormat fmt = static_cast<VkFormat>(sw->format);

    // Overlay-on-load pass: use after blitting scene to swapchain image.
    VkAttachmentDescription attLoad = {};
    attLoad.format = fmt;
    attLoad.samples = VK_SAMPLE_COUNT_1_BIT;
    attLoad.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attLoad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attLoad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attLoad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attLoad.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attLoad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attLoad;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &rp) != VK_SUCCESS) return;
    sw->renderPass = rp;

    // Overlay-clear pass: use when no scene blit (clear then draw overlay).
    VkAttachmentDescription attClear = {};
    attClear.format = fmt;
    attClear.samples = VK_SAMPLE_COUNT_1_BIT;
    attClear.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attClear.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attClear.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attClear.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attClear.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attClear.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    rpInfo.pAttachments = &attClear;
    VkRenderPass rpClear = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &rpClear) != VK_SUCCESS) {
        vkDestroyRenderPass(device, rp, nullptr);
        sw->renderPass = nullptr;
        return;
    }
    sw->renderPassOverlayClear = rpClear;

    sw->framebuffers.resize(sw->imageViews.size());
    sw->framebuffersOverlayClear.resize(sw->imageViews.size());
    for (size_t i = 0; i < sw->imageViews.size(); ++i) {
        VkImageView view = static_cast<VkImageView>(sw->imageViews[i]);
        if (view == VK_NULL_HANDLE) continue;
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &view;
        fbInfo.width = sw->width;
        fbInfo.height = sw->height;
        fbInfo.layers = 1;
        fbInfo.renderPass = rp;
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &fb) == VK_SUCCESS)
            sw->framebuffers[i] = fb;
        fbInfo.renderPass = rpClear;
        fb = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &fb) == VK_SUCCESS)
            sw->framebuffersOverlayClear[i] = fb;
    }
}

bool SwapchainVK::Create(SwapchainVK* out, void* device, void* physicalDevice, void* surface, uint32_t width, uint32_t height) {
    if (!out || !device || !physicalDevice || !surface) return false;
    width = std::max(1u, width);
    height = std::max(1u, height);
    VkDevice dev = static_cast<VkDevice>(device);
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(physicalDevice);
    VkSurfaceKHR surf = static_cast<VkSurfaceKHR>(surface);

    VkSurfaceCapabilitiesKHR caps = {};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK::Create: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (0x%x)", res);
        return false;
    }
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &formatCount, formats.data());
    VkSurfaceFormatKHR chosenFormat = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, presentModes.data());
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
    swapInfo.surface = surf;
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
    res = vkCreateSwapchainKHR(dev, &swapInfo, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK::Create: vkCreateSwapchainKHR failed (0x%x)", res);
        return false;
    }

    out->device = device;
    out->physicalDevice = physicalDevice;
    out->surface = surface;
    out->swapchain = swapchain;
    out->width = extent.width;
    out->height = extent.height;
    out->format = static_cast<uint32_t>(chosenFormat.format);

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(dev, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(dev, swapchain, &swapchainImageCount, swapchainImages.data());
    out->images.reserve(swapchainImageCount);
    for (VkImage img : swapchainImages) out->images.push_back(img);
    out->imageViews.resize(swapchainImageCount);
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
        if (vkCreateImageView(dev, &viewInfo, nullptr, &imageView) == VK_SUCCESS)
            out->imageViews[i] = imageView;
    }
    EnsureRenderPassAndFramebuffers(out);
    if (!CreateRenderFinishedSemaphores(out)) {
        DestroyRenderFinishedSemaphores(out);
        Destroy(out, device);
        return false;
    }
    return true;
}

void SwapchainVK::Destroy(SwapchainVK* sw, void* device) {
    if (!sw || !device) return;
    DestroyRenderFinishedSemaphores(sw);
    VkDevice dev = static_cast<VkDevice>(device);
    for (void* fb : sw->framebuffersOverlayClear) {
        if (fb) vkDestroyFramebuffer(dev, static_cast<VkFramebuffer>(fb), nullptr);
    }
    sw->framebuffersOverlayClear.clear();
    for (void* fb : sw->framebuffers) {
        if (fb) vkDestroyFramebuffer(dev, static_cast<VkFramebuffer>(fb), nullptr);
    }
    sw->framebuffers.clear();
    VkRenderPass rpClear = static_cast<VkRenderPass>(sw->renderPassOverlayClear);
    if (rpClear != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, rpClear, nullptr);
        sw->renderPassOverlayClear = nullptr;
    }
    VkRenderPass rp = static_cast<VkRenderPass>(sw->renderPass);
    if (rp != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, rp, nullptr);
        sw->renderPass = nullptr;
    }
    for (void* p : sw->imageViews) {
        if (p) vkDestroyImageView(dev, static_cast<VkImageView>(p), nullptr);
    }
    sw->imageViews.clear();
    sw->images.clear();
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(sw->swapchain);
    if (sc != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, sc, nullptr);
        sw->swapchain = nullptr;
    }
    sw->width = 0;
    sw->height = 0;
    sw->format = 0;
}

void SwapchainVK::Resize(uint32_t w, uint32_t h, bool forceRecreate) {
    w = std::max(1u, w);
    h = std::max(1u, h);
    if (!forceRecreate && w == width && h == height) return;
    if (!device || !physicalDevice || !surface) return;
    VkDevice dev = static_cast<VkDevice>(device);
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(physicalDevice);
    VkSurfaceKHR surf = static_cast<VkSurfaceKHR>(surface);
    vkDeviceWaitIdle(dev);
    DestroyRenderFinishedSemaphores(this);

    for (void* fb : framebuffersOverlayClear) {
        if (fb) vkDestroyFramebuffer(dev, static_cast<VkFramebuffer>(fb), nullptr);
    }
    framebuffersOverlayClear.clear();
    for (void* fb : framebuffers) {
        if (fb) vkDestroyFramebuffer(dev, static_cast<VkFramebuffer>(fb), nullptr);
    }
    framebuffers.clear();
    VkRenderPass rpClear = static_cast<VkRenderPass>(renderPassOverlayClear);
    if (rpClear != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, rpClear, nullptr);
        renderPassOverlayClear = nullptr;
    }
    VkRenderPass rp = static_cast<VkRenderPass>(renderPass);
    if (rp != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, rp, nullptr);
        renderPass = nullptr;
    }
    for (void* view : imageViews) {
        if (view) vkDestroyImageView(dev, static_cast<VkImageView>(view), nullptr);
    }
    imageViews.clear();
    images.clear();
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(swapchain);
    if (sc != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, sc, nullptr);
        swapchain = nullptr;
    }

    VkSurfaceCapabilitiesKHR caps = {};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK::Resize: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (0x%x)", res);
        return;
    }
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &formatCount, formats.data());
    VkSurfaceFormatKHR chosenFormat = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR pm : presentModes) {
        if (pm == VK_PRESENT_MODE_FIFO_KHR) { presentMode = pm; break; }
    }
    VkExtent2D extent = {};
    extent.width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swapInfo = {};
    swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface = surf;
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

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    res = vkCreateSwapchainKHR(dev, &swapInfo, nullptr, &newSwapchain);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK::Resize: vkCreateSwapchainKHR failed (0x%x)", res);
        return;
    }
    swapchain = newSwapchain;
    this->width = extent.width;
    this->height = extent.height;
    format = static_cast<uint32_t>(chosenFormat.format);

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(dev, newSwapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(dev, newSwapchain, &swapchainImageCount, swapchainImages.data());
    images.reserve(swapchainImageCount);
    for (VkImage img : swapchainImages) images.push_back(img);
    imageViews.resize(swapchainImageCount);
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
        if (vkCreateImageView(dev, &viewInfo, nullptr, &imageView) == VK_SUCCESS)
            imageViews[i] = imageView;
    }
    EnsureRenderPassAndFramebuffers(this);
    CreateRenderFinishedSemaphores(this);
}

int SwapchainVK::Acquire(void* semaphoreToSignal, uint64_t timeoutMs, uint32_t* outImageIndex) {
    if (!device || !swapchain || !outImageIndex) return static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
    VkDevice dev = static_cast<VkDevice>(device);
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(swapchain);
    VkSemaphore sem = semaphoreToSignal ? static_cast<VkSemaphore>(semaphoreToSignal) : VK_NULL_HANDLE;
    uint64_t timeout = (timeoutMs == UINT64_MAX) ? UINT64_MAX : (timeoutMs * 1000000);
    VkResult res = vkAcquireNextImageKHR(dev, sc, timeout, sem, VK_NULL_HANDLE, outImageIndex);
    return static_cast<int>(res);
}

int SwapchainVK::Present(void* queue, void* waitSemaphore, uint32_t imageIndex) {
    if (!swapchain) return static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
    if (imageIndex >= images.size()) return static_cast<int>(VK_ERROR_UNKNOWN);
    VkQueue q = static_cast<VkQueue>(queue);
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(swapchain);
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &sc;
    presentInfo.pImageIndices = &imageIndex;
    VkSemaphore waitSem = waitSemaphore ? static_cast<VkSemaphore>(waitSemaphore) : VK_NULL_HANDLE;
    if (waitSem != VK_NULL_HANDLE) {
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &waitSem;
    }
    VkDevice dev = static_cast<VkDevice>(device);
    PFN_vkQueuePresentKHR pfnPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
    if (!pfnPresent) return static_cast<int>(VK_ERROR_EXTENSION_NOT_PRESENT);
    VkResult res = pfnPresent(q, &presentInfo);
    return static_cast<int>(res);
}

void* SwapchainVK::GetRenderFinishedSemaphore(uint32_t imageIndex) const {
    return imageIndex < renderFinishedSemaphores.size() ? renderFinishedSemaphores[imageIndex] : nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
