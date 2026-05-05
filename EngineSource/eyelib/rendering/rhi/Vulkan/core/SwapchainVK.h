#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** **Swapchain** and **per-image** helpers for presentation and ImGui-style overlay passes.
 *
 * **Lifecycle:** **`Create`** / **`Resize`** pick a **`VkPresentModeKHR`** from **`SwapchainPresentMode`** (FIFO default; mailbox/immediate when listed),
 *  **BGRA8** + **SRGB_NONLINEAR** when listed,
 * **`minImageCount + 1`** (clamped to **`maxImageCount`**), **`COLOR_ATTACHMENT | TRANSFER_DST`** usage (scene **blit** / **clear** to swapchain).
 * **`Resize`** calls **`vkDeviceWaitIdle`**, destroys the old swapchain and rebuilds views, **two** **`VkRenderPass`**es (load vs clear for overlay),
 * and **one binary semaphore per image** (**`renderFinishedSemaphores`**) so **present** waits on the semaphore for **that** image (avoids VU 00067-class reuse).
 *
 * **Acquire / present:** Callers treat **`VK_SUCCESS`** and **`VK_SUBOPTIMAL_KHR`** from **`Acquire`** as usable **`outImageIndex`**;
 * **`VK_ERROR_OUT_OF_DATE_KHR`** means recreate (**`DeviceVK`** calls **`Resize(..., true)`**). **`Present`** uses **`vkGetDeviceProcAddr`** for **`vkQueuePresentKHR`**.
 *
 * **Resolve path:** HDR/scene → swapchain may use **`vkCmdCopyImage`** when formats match, else **`present_resolve`** in **`DeviceVK::BlitToDefaultFramebuffer`**
 * (`DeviceVKPresentation.cpp`) with barriers and **`PRESENT_SRC`** before present. */
struct SwapchainVK {
    void* swapchain = nullptr;           // VkSwapchainKHR
    std::vector<void*> images;          // VkImage per swapchain image
    std::vector<void*> imageViews;       // VkImageView per image
    std::vector<void*> framebuffers;    // VkFramebuffer per image (for overlay-on-load pass)
    std::vector<void*> framebuffersOverlayClear;  // VkFramebuffer per image (for clear-then-overlay pass)
    void* renderPass = nullptr;          // VkRenderPass (LOAD_OP_LOAD; use after blit)
    void* renderPassOverlayClear = nullptr;  // VkRenderPass (LOAD_OP_CLEAR; use when no scene blit)
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;                // VkFormat (opaque)
    uint32_t colorSpace = 0;            // VkColorSpaceKHR (opaque)
    /** Per-image semaphores signaled by submit, waited by present. Avoids reusing a semaphore still in use by present (validation 00067). */
    std::vector<void*> renderFinishedSemaphores;

    void* device = nullptr;              // VkDevice (for Resize/Destroy)
    void* physicalDevice = nullptr;       // VkPhysicalDevice (for Resize)
    void* surface = nullptr;             // VkSurfaceKHR (for Resize)
    /** When true, swapchain create/resize chains `VkSwapchainPresentModesCreateInfoEXT` (requires `VK_EXT_swapchain_maintenance1`). */
    bool useSwapchainMaintenance1 = false;
    /** When true, `Present` chains `VkPresentIdKHR` (requires `VK_KHR_present_id` + device features). */
    bool usePresentId = false;
    uint64_t nextPresentId = 1;
    uint64_t lastPresentId = 0;

    /** Create swapchain, images, views, render pass, framebuffers. Returns true on success.
     *  If **`debugNameDevice`** is set, applies **`VK_EXT_debug_utils`** name **`OpenEye_Swapchain`** to **`VkSwapchainKHR`**. */
    static bool Create(SwapchainVK* out, void* device, void* physicalDevice, void* surface, uint32_t width, uint32_t height,
                       DeviceVK* debugNameDevice = nullptr, SwapchainPresentMode presentPreference = SwapchainPresentMode::Fifo,
                       bool useSwapchainMaintenance1 = false);
    /** Destroy framebuffers, render pass, views, swapchain. Does not destroy surface.
     *  **`maintenanceDevice`:** when non-null and maintenance1 is active, calls `ReleaseSwapchainImagesForMaintenance` before destroying `VkSwapchainKHR`. */
    static void Destroy(SwapchainVK* sw, void* device, DeviceVK* maintenanceDevice = nullptr);
    /** Recreate swapchain and derived state. When forceRecreate is true, recreates even if width/height unchanged (e.g. OUT_OF_DATE).
     *  **`debugNameDevice`:** optional **`VK_EXT_debug_utils`** naming for the new swapchain (same as **`Create`**). */
    void Resize(uint32_t width, uint32_t height, bool forceRecreate = false, DeviceVK* debugNameDevice = nullptr,
                 SwapchainPresentMode presentPreference = SwapchainPresentMode::Fifo);

    /** Acquire next image; signal semaphore when available. Returns VkResult; outImageIndex valid only on VK_SUCCESS/VK_SUBOPTIMAL_KHR. */
    int Acquire(void* semaphoreToSignal, uint64_t timeoutMs, uint32_t* outImageIndex);
    /** Present image; wait on semaphore before presenting. Returns VkResult (handle VK_ERROR_OUT_OF_DATE_KHR). */
    int Present(void* queue, void* waitSemaphore, uint32_t imageIndex);

    uint32_t GetImageCount() const { return static_cast<uint32_t>(images.size()); }
    void* GetRenderPass() const { return renderPass; }
    /** Render pass for overlay when swapchain image was not blitted (clear then draw). */
    void* GetOverlayClearRenderPass() const { return renderPassOverlayClear; }
    void* GetOverlayClearFramebuffer(uint32_t i) const { return i < framebuffersOverlayClear.size() ? framebuffersOverlayClear[i] : nullptr; }
    void* GetImage(uint32_t i) const { return i < images.size() ? images[i] : nullptr; }
    void* GetImageView(uint32_t i) const { return i < imageViews.size() ? imageViews[i] : nullptr; }
    void* GetFramebuffer(uint32_t i) const { return i < framebuffers.size() ? framebuffers[i] : nullptr; }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    /** Extent (width, height) for renderer. */
    void GetExtent(uint32_t* outWidth, uint32_t* outHeight) const { if (outWidth) *outWidth = width; if (outHeight) *outHeight = height; }
    uint32_t GetFormat() const { return format; }
    uint32_t GetColorSpace() const { return colorSpace; }
    uint64_t GetLastPresentId() const { return lastPresentId; }
    /** Semaphore for this image index (signal on submit, wait on present). Valid after Create/Resize. */
    void* GetRenderFinishedSemaphore(uint32_t imageIndex) const;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
