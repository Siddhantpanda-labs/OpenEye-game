#include "eyelib/render/rhi/Vulkan/CreateDeviceVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateVulkanDevice(const VulkanDeviceCreateInfo& info) {
    if (!info.instance || !info.physicalDevice || !info.device || !info.graphicsQueue) return nullptr;
    return std::make_unique<DeviceVk>(info);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
