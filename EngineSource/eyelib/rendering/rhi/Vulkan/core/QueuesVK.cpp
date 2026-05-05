#include "eyelib/rendering/rhi/Vulkan/core/QueuesVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

bool QueuesVK::Create(QueuesVK* out, void* device, uint32_t graphicsFamily, uint32_t computeFamily, uint32_t transferFamily) {
    if (!out || !device) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    VkQueue g = VK_NULL_HANDLE, c = VK_NULL_HANDLE, t = VK_NULL_HANDLE;
    vkGetDeviceQueue(dev, graphicsFamily, 0, &g);
    vkGetDeviceQueue(dev, computeFamily, 0, &c);
    vkGetDeviceQueue(dev, transferFamily, 0, &t);
    out->graphics = g;
    out->compute = c;
    out->transfer = t;
    out->graphicsFamily = graphicsFamily;
    out->computeFamily = computeFamily;
    out->transferFamily = transferFamily;
    return (g != VK_NULL_HANDLE);
}

void QueuesVK::Destroy(QueuesVK* queues, void* device) {
    (void)device;
    if (!queues) return;
    queues->graphics = nullptr;
    queues->compute = nullptr;
    queues->transfer = nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
