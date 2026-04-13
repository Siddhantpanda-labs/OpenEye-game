#pragma once

#include "eyelib/rendering/rhi/Vulkan/commands/CommandBufferVK.h"
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** Per-frame GPU state: command pool, primary command buffer, fence, and swapchain sync semaphores. Opaque handles (Vk*).
 *  Must stay in sync with `DeviceVK::kMaxFrames` (3) and `UploadHeapVK::kMaxFrames` / `DeletionQueueVK::kMaxFrames`.
 *
 *  **Fence:** Created **signaled** (`VK_FENCE_CREATE_SIGNALED_BIT`) so the first `vkWaitForFences` in `BeginFrame` does not block.
 *  After each `EndFrame` submit, the fence is **unsignaled** until GPU finishes; the next time this ring index comes around,
 *  `BeginFrame` waits before resetting the command pool.
 *
 *  **Semaphores:** `imageAvailable` is waited in `vkQueueSubmit` (color attachment stage) when presenting; `renderFinished`
 *  (or a per-swapchain-image semaphore) is signaled by that submit and waited by `vkQueuePresentKHR`. */
struct FrameContextVK {
    void* commandPool = nullptr;       // VkCommandPool
    CommandBufferVK commandBuffer;     // primary, one per frame
    void* fence = nullptr;             // VkFence (signaled when frame's submit completes)
    void* imageAvailable = nullptr;   // VkSemaphore (signaled by vkAcquireNextImageKHR)
    void* renderFinished = nullptr;    // VkSemaphore (signaled by submit; waited by present)
    uint32_t swapchainImageIndex = 0xFFFFFFFFu;  // set by Acquire in BeginFrame; frame owns image for recording/present

    /** Create pool, buffer, fence, and two binary semaphores. Returns true on success. */
    static bool Create(FrameContextVK* out, void* device, uint32_t queueFamilyIndex);
    /** Free buffer, destroy pool, fence, and semaphores. */
    static void Destroy(FrameContextVK* ctx, void* device);
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
