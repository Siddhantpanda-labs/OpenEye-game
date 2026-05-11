#include "eyelib/rendering/rhi/Vulkan/core/SwapchainVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

VkSurfaceFormatKHR SelectSdrSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : formats[0];
}

VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats, DeviceVK* deviceSettings) {
    SwapchainOutputMode mode = SwapchainOutputMode::SdrSrgb;
    bool csExt = false;
    if (deviceSettings) {
        mode = deviceSettings->GetSwapchainOutputMode();
        csExt = deviceSettings->SupportsSwapchainColorspace();
    }
    if (formats.empty())
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    if (!csExt || mode == SwapchainOutputMode::SdrSrgb) {
        const VkSurfaceFormatKHR sdr = SelectSdrSurfaceFormat(formats);
        static bool loggedSdrFormat = false;
        if (!loggedSdrFormat) {
            loggedSdrFormat = true;
            core::Log::InfoF("SwapchainVK: SDR surface format 0x%x colorSpace 0x%x.",
                             static_cast<unsigned>(sdr.format), static_cast<unsigned>(sdr.colorSpace));
        }
        return sdr;
    }

    if (mode == SwapchainOutputMode::Hdr10St2084) {
        for (const auto& f : formats) {
            if (f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
                core::Log::InfoF("SwapchainVK: HDR10 ST2084 surface format 0x%x colorSpace 0x%x.",
                                 static_cast<unsigned>(f.format), static_cast<unsigned>(f.colorSpace));
                return f;
            }
        }
        for (const auto& f : formats) {
            if (f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                core::Log::InfoF("SwapchainVK: HDR10 ST2084 surface format 0x%x colorSpace 0x%x.",
                                 static_cast<unsigned>(f.format), static_cast<unsigned>(f.colorSpace));
                return f;
            }
        }
        core::Log::Warn("SwapchainVK: HDR10 output requested but no HDR10 surface format found; falling back to SDR.");
        return SelectSdrSurfaceFormat(formats);
    }

    if (mode == SwapchainOutputMode::HdrExtendedSrgbLinear) {
        for (const auto& f : formats) {
            if (f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT && f.format == VK_FORMAT_R16G16B16A16_SFLOAT) {
                core::Log::InfoF("SwapchainVK: extended sRGB linear (FP16) surface format 0x%x colorSpace 0x%x.",
                                 static_cast<unsigned>(f.format), static_cast<unsigned>(f.colorSpace));
                return f;
            }
        }
        for (const auto& f : formats) {
            if (f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
                core::Log::InfoF("SwapchainVK: extended sRGB linear surface format 0x%x colorSpace 0x%x.",
                                 static_cast<unsigned>(f.format), static_cast<unsigned>(f.colorSpace));
                return f;
            }
        }
        core::Log::Warn("SwapchainVK: scRGB / extended linear output requested but no matching surface format; falling back to SDR.");
        return SelectSdrSurfaceFormat(formats);
    }

    return SelectSdrSurfaceFormat(formats);
}

bool PresentModeListed(const std::vector<VkPresentModeKHR>& modes, VkPresentModeKHR m) {
    return std::find(modes.begin(), modes.end(), m) != modes.end();
}

const char* PresentModeName(VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR: return "Immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR: return "Mailbox";
    case VK_PRESENT_MODE_FIFO_KHR: return "Fifo";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FifoRelaxed";
    default: return "Other";
    }
}

VkPresentModeKHR PreferredVkPresentMode(SwapchainPresentMode pref) {
    switch (pref) {
    case SwapchainPresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
    case SwapchainPresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case SwapchainPresentMode::Fifo:
    default: return VK_PRESENT_MODE_FIFO_KHR;
    }
}

VkPresentModeKHR SelectVkPresentMode(const std::vector<VkPresentModeKHR>& modes, SwapchainPresentMode pref) {
    if (modes.empty()) return VK_PRESENT_MODE_FIFO_KHR;
    switch (pref) {
    case SwapchainPresentMode::Mailbox:
        if (PresentModeListed(modes, VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
        if (PresentModeListed(modes, VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;
    case SwapchainPresentMode::Immediate:
        if (PresentModeListed(modes, VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (PresentModeListed(modes, VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
        break;
    case SwapchainPresentMode::Fifo:
    default:
        break;
    }
    if (PresentModeListed(modes, VK_PRESENT_MODE_FIFO_KHR)) return VK_PRESENT_MODE_FIFO_KHR;
    return modes[0];
}

uint32_t SelectSwapchainImageCount(const VkSurfaceCapabilitiesKHR& caps, VkPresentModeKHR vkMode) {
    uint32_t n = caps.minImageCount + 1;
    if (vkMode == VK_PRESENT_MODE_MAILBOX_KHR)
        n = (std::max)({caps.minImageCount + 1u, 3u});
    if (caps.maxImageCount > 0 && n > caps.maxImageCount) n = caps.maxImageCount;
    if (n < caps.minImageCount) n = caps.minImageCount;
    return n;
}

} // namespace

/** Create one binary semaphore per swapchain image for render-finished (submit signals, present waits). */
static bool CreateRenderFinishedSemaphores(SwapchainVK* sw) {
    if (!sw || !sw->device || sw->images.empty()) return false;
    VkDevice dev = static_cast<VkDevice>(sw->device);
    sw->renderFinishedSemaphores.resize(sw->images.size(), nullptr);
    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (size_t i = 0; i < sw->images.size(); ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        VkResult sr = vkCreateSemaphore(dev, &semInfo, nullptr, &sem);
        if (sr != VK_SUCCESS) {
            core::Log::ErrorF("SwapchainVK: vkCreateSemaphore (renderFinished) image %zu failed (0x%x)", i, static_cast<unsigned>(sr));
            for (size_t j = 0; j < i; ++j) {
                if (sw->renderFinishedSemaphores[j])
                    vkDestroySemaphore(dev, static_cast<VkSemaphore>(sw->renderFinishedSemaphores[j]), nullptr);
                sw->renderFinishedSemaphores[j] = nullptr;
            }
            sw->renderFinishedSemaphores.clear();
            return false;
        }
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
    VkResult rpRes = vkCreateRenderPass(device, &rpInfo, nullptr, &rp);
    if (rpRes != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK: vkCreateRenderPass (overlay LOAD) failed (0x%x)", static_cast<unsigned>(rpRes));
        return;
    }
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
    rpRes = vkCreateRenderPass(device, &rpInfo, nullptr, &rpClear);
    if (rpRes != VK_SUCCESS) {
        core::Log::ErrorF("SwapchainVK: vkCreateRenderPass (overlay CLEAR) failed (0x%x)", static_cast<unsigned>(rpRes));
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

bool SwapchainVK::Create(SwapchainVK* out, void* device, void* physicalDevice, void* surface, uint32_t width, uint32_t height,
                         DeviceVK* debugNameDevice, SwapchainPresentMode presentPreference, bool useSwapchainMaintenance1) {
    if (!out || !device || !physicalDevice || !surface) return false;
    out->useSwapchainMaintenance1 = useSwapchainMaintenance1;
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
    const VkSurfaceFormatKHR chosenFormat = SelectSurfaceFormat(formats, debugNameDevice);
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, presentModes.data());
    const VkPresentModeKHR presentMode = SelectVkPresentMode(presentModes, presentPreference);
    const VkPresentModeKHR preferredMode = PreferredVkPresentMode(presentPreference);
    if (presentMode != preferredMode) {
        core::Log::WarnF("SwapchainVK::Create: preferred present mode %s unavailable; selected %s fallback.",
                         PresentModeName(preferredMode), PresentModeName(presentMode));
    } else {
        core::Log::InfoF("SwapchainVK::Create: selected present mode %s.", PresentModeName(presentMode));
    }
    VkExtent2D extent = {};
    extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    uint32_t imageCount = SelectSwapchainImageCount(caps, presentMode);
    core::Log::InfoF("SwapchainVK::Create: imageCount=%u (caps min=%u max=%u).",
                     imageCount, caps.minImageCount, caps.maxImageCount);

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
#if defined(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT)
    VkSwapchainPresentModesCreateInfoEXT presentModesExt = {};
    if (useSwapchainMaintenance1) {
        presentModesExt.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT;
        presentModesExt.presentModeCount = 1u;
        presentModesExt.pPresentModes = &presentMode;
        swapInfo.pNext = &presentModesExt;
    }
#else
    (void)useSwapchainMaintenance1;
#endif

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
    out->colorSpace = static_cast<uint32_t>(chosenFormat.colorSpace);

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
        Destroy(out, device, debugNameDevice);
        return false;
    }
    if (debugNameDevice && out->swapchain)
        SetDebugNameSwapchain(debugNameDevice, out->swapchain, "OpenEye_Swapchain");
    out->usePresentId = debugNameDevice && debugNameDevice->SupportsPresentWait();
    out->nextPresentId = 1;
    out->lastPresentId = 0;
    return true;
}

void SwapchainVK::Destroy(SwapchainVK* sw, void* device, DeviceVK* maintenanceDevice) {
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
    const uint32_t imageCountForRelease = static_cast<uint32_t>(sw->images.size());
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(sw->swapchain);
    if (sc != VK_NULL_HANDLE && imageCountForRelease > 0 && maintenanceDevice && sw->useSwapchainMaintenance1)
        maintenanceDevice->ReleaseSwapchainImagesForMaintenance(sw->swapchain, imageCountForRelease);
    if (maintenanceDevice) {
        BarrierTrackerVK* tr = maintenanceDevice->GetBarrierTrackerVK();
        for (void* img : sw->images)
            if (img) BarrierTrackerVK::RemoveImageState(tr, img);
    }
    sw->images.clear();
    if (sc != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, sc, nullptr);
        sw->swapchain = nullptr;
    }
    sw->width = 0;
    sw->height = 0;
    sw->format = 0;
    sw->colorSpace = 0;
}

void SwapchainVK::Resize(uint32_t w, uint32_t h, bool forceRecreate, DeviceVK* debugNameDevice,
                         SwapchainPresentMode presentPreference) {
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
    const uint32_t imageCountForRelease = static_cast<uint32_t>(images.size());
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(swapchain);
    if (sc != VK_NULL_HANDLE && imageCountForRelease > 0 && debugNameDevice && useSwapchainMaintenance1)
        debugNameDevice->ReleaseSwapchainImagesForMaintenance(swapchain, imageCountForRelease);
    if (debugNameDevice) {
        BarrierTrackerVK* tr = debugNameDevice->GetBarrierTrackerVK();
        for (void* img : images)
            if (img) BarrierTrackerVK::RemoveImageState(tr, img);
    }
    images.clear();
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
    const VkSurfaceFormatKHR chosenFormat = SelectSurfaceFormat(formats, debugNameDevice);
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &presentModeCount, presentModes.data());
    const VkPresentModeKHR presentMode = SelectVkPresentMode(presentModes, presentPreference);
    const VkPresentModeKHR preferredMode = PreferredVkPresentMode(presentPreference);
    if (presentMode != preferredMode) {
        static bool loggedFallback = false;
        if (!loggedFallback) {
            loggedFallback = true;
            core::Log::WarnF("SwapchainVK::Resize: preferred present mode %s unavailable; selected %s fallback.",
                             PresentModeName(preferredMode), PresentModeName(presentMode));
        }
    } else {
        static bool loggedPresentMode = false;
        if (!loggedPresentMode) {
            loggedPresentMode = true;
            core::Log::InfoF("SwapchainVK::Resize: selected present mode %s.", PresentModeName(presentMode));
        }
    }
    VkExtent2D extent = {};
    extent.width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    uint32_t imageCount = SelectSwapchainImageCount(caps, presentMode);
    static bool loggedImageCount = false;
    if (!loggedImageCount) {
        loggedImageCount = true;
        core::Log::InfoF("SwapchainVK::Resize: imageCount=%u (caps min=%u max=%u).",
                         imageCount, caps.minImageCount, caps.maxImageCount);
    }

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
#if defined(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT)
    VkSwapchainPresentModesCreateInfoEXT presentModesExtResize = {};
    if (useSwapchainMaintenance1) {
        presentModesExtResize.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT;
        presentModesExtResize.presentModeCount = 1u;
        presentModesExtResize.pPresentModes = &presentMode;
        swapInfo.pNext = &presentModesExtResize;
    }
#endif

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
    colorSpace = static_cast<uint32_t>(chosenFormat.colorSpace);

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
    if (debugNameDevice && swapchain)
        SetDebugNameSwapchain(debugNameDevice, swapchain, "OpenEye_Swapchain");
    usePresentId = debugNameDevice && debugNameDevice->SupportsPresentWait();
    nextPresentId = 1;
    lastPresentId = 0;
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
    uint64_t presentIdValue = 0;
#if defined(VK_STRUCTURE_TYPE_PRESENT_ID_KHR)
    VkPresentIdKHR presentIdKHR = {};
    if (usePresentId) {
        presentIdValue = nextPresentId++;
        presentIdKHR.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
        presentIdKHR.pNext = nullptr;
        presentIdKHR.swapchainCount = 1;
        presentIdKHR.pPresentIds = &presentIdValue;
        presentInfo.pNext = &presentIdKHR;
    }
#else
    (void)presentIdValue;
#endif
    VkDevice dev = static_cast<VkDevice>(device);
    PFN_vkQueuePresentKHR pfnPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
    if (!pfnPresent) return static_cast<int>(VK_ERROR_EXTENSION_NOT_PRESENT);
    VkResult res = pfnPresent(q, &presentInfo);
#if defined(VK_STRUCTURE_TYPE_PRESENT_ID_KHR)
    if (usePresentId && (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR))
        lastPresentId = presentIdValue;
#endif
    if (res != VK_SUCCESS) {
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
            core::Log::WarnF("SwapchainVK::Present: vkQueuePresentKHR returned recoverable status (0x%x).", static_cast<unsigned>(res));
        } else {
            core::Log::ErrorF("SwapchainVK::Present: vkQueuePresentKHR failed (0x%x).", static_cast<unsigned>(res));
        }
    }
    return static_cast<int>(res);
}

void* SwapchainVK::GetRenderFinishedSemaphore(uint32_t imageIndex) const {
    return imageIndex < renderFinishedSemaphores.size() ? renderFinishedSemaphores[imageIndex] : nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
