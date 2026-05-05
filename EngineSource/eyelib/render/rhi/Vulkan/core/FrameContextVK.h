#pragma once

#include "eyelib/render/rhi/Vulkan/commands/CommandBufferVK.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Per-frame GPU state: command pool, primary command buffer, fence, and swapchain sync semaphores. Opaque handles (Vk*). */
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
} // namespace render
} // namespace eyelib
