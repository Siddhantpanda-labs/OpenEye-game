#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

/** Swapchain and per-image resources (views, framebuffers, render pass). Opaque Vk* handles. */
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
    uint32_t currentImageIndex = 0xFFFFFFFFu;  // set by Acquire(), used by Present()

    void* device = nullptr;              // VkDevice (for Resize/Destroy)
    void* physicalDevice = nullptr;       // VkPhysicalDevice (for Resize)
    void* surface = nullptr;             // VkSurfaceKHR (for Resize)

    /** Create swapchain, images, views, render pass, framebuffers. Returns true on success. */
    static bool Create(SwapchainVK* out, void* device, void* physicalDevice, void* surface, uint32_t width, uint32_t height);
    /** Destroy framebuffers, render pass, views, swapchain. Does not destroy surface. */
    static void Destroy(SwapchainVK* sw, void* device);
    /** Recreate swapchain and derived state for new size. Call when window resizes. */
    void Resize(uint32_t width, uint32_t height);

    /** Acquire next image; signal semaphore when available. Returns true on success/suboptimal. Sets currentImageIndex. */
    bool Acquire(void* semaphoreToSignal, uint64_t timeoutMs = UINT64_MAX);
    /** Present current image; wait on semaphore before presenting. No vkQueueWaitIdle. */
    bool Present(void* queue, void* waitSemaphore);

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
    uint32_t GetCurrentImageIndex() const { return currentImageIndex; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
