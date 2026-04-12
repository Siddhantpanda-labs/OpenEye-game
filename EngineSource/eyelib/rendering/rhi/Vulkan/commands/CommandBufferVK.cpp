#include "eyelib/rendering/rhi/Vulkan/commands/CommandBufferVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

bool CommandBufferVK::Allocate(CommandBufferVK* out, void* device, void* commandPool) {
    if (!out || !device || !commandPool) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    VkCommandPool pool = static_cast<VkCommandPool>(commandPool);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer buf = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(dev, &allocInfo, &buf) != VK_SUCCESS)
        return false;
    out->handle = buf;
    return true;
}

void CommandBufferVK::Free(CommandBufferVK* buf, void* device, void* commandPool) {
    if (!buf || !device || !commandPool || !buf->handle) return;
    VkCommandBuffer b = static_cast<VkCommandBuffer>(buf->handle);
    vkFreeCommandBuffers(static_cast<VkDevice>(device), static_cast<VkCommandPool>(commandPool), 1, &b);
    buf->handle = nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
