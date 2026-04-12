#include "eyelib/rendering/rhi/Vulkan/commands/CommandPoolVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

bool CommandPoolVK::Create(CommandPoolVK* out, void* device, uint32_t queueFamilyIndex) {
    if (!out || !device) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(dev, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;
    out->handle = pool;
    return true;
}

void CommandPoolVK::Destroy(CommandPoolVK* pool, void* device) {
    if (!pool || !device) return;
    VkCommandPool p = static_cast<VkCommandPool>(pool->handle);
    if (p != VK_NULL_HANDLE) {
        vkDestroyCommandPool(static_cast<VkDevice>(device), p, nullptr);
        pool->handle = nullptr;
    }
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
