#include "eyelib/render/rhi/Vulkan/core/FrameContextVK.h"
#include "eyelib/render/rhi/Vulkan/commands/CommandBufferVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

bool FrameContextVK::Create(FrameContextVK* out, void* device, uint32_t queueFamilyIndex) {
    if (!out || !device) return false;
    out->commandPool = nullptr;
    out->commandBuffer.handle = nullptr;
    out->fence = nullptr;
    out->imageAvailable = nullptr;
    out->renderFinished = nullptr;

    VkDevice dev = static_cast<VkDevice>(device);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(dev, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;
    out->commandPool = pool;

    if (!CommandBufferVK::Allocate(&out->commandBuffer, device, pool)) {
        vkDestroyCommandPool(dev, pool, nullptr);
        out->commandPool = nullptr;
        return false;
    }

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(dev, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        CommandBufferVK::Free(&out->commandBuffer, device, pool);
        vkDestroyCommandPool(dev, pool, nullptr);
        out->commandPool = nullptr;
        return false;
    }
    out->fence = fence;

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    if (vkCreateSemaphore(dev, &semInfo, nullptr, &imageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(dev, &semInfo, nullptr, &renderFinished) != VK_SUCCESS) {
        vkDestroyFence(dev, fence, nullptr);
        CommandBufferVK::Free(&out->commandBuffer, device, pool);
        vkDestroyCommandPool(dev, pool, nullptr);
        out->commandPool = nullptr;
        out->fence = nullptr;
        return false;
    }
    out->imageAvailable = imageAvailable;
    out->renderFinished = renderFinished;

    return true;
}

void FrameContextVK::Destroy(FrameContextVK* ctx, void* device) {
    if (!ctx || !device) return;
    VkDevice dev = static_cast<VkDevice>(device);
    VkCommandPool pool = static_cast<VkCommandPool>(ctx->commandPool);
    VkFence fence = static_cast<VkFence>(ctx->fence);
    VkSemaphore imgAvail = static_cast<VkSemaphore>(ctx->imageAvailable);
    VkSemaphore renderFin = static_cast<VkSemaphore>(ctx->renderFinished);
    CommandBufferVK::Free(&ctx->commandBuffer, device, ctx->commandPool);
    if (pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(dev, pool, nullptr);
    if (fence != VK_NULL_HANDLE)
        vkDestroyFence(dev, fence, nullptr);
    if (imgAvail != VK_NULL_HANDLE)
        vkDestroySemaphore(dev, imgAvail, nullptr);
    if (renderFin != VK_NULL_HANDLE)
        vkDestroySemaphore(dev, renderFin, nullptr);
    ctx->commandPool = nullptr;
    ctx->commandBuffer.handle = nullptr;
    ctx->fence = nullptr;
    ctx->imageAvailable = nullptr;
    ctx->renderFinished = nullptr;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
