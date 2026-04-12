#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"

namespace eyelib {
namespace rendering {
namespace rhi {

std::unique_ptr<IDevice> CreateVulkanDevice(void* windowHandle, uint32_t width, uint32_t height) {
    auto device = std::make_unique<DeviceVK>();
    if (!device->Initialize(windowHandle, width, height))
        return nullptr;
    return device;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
