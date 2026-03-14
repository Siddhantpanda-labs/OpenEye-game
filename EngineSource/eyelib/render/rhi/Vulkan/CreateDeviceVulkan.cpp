#include "eyelib/render/rhi/Vulkan/CreateDeviceVulkan.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateVulkanDevice(void* windowHandle, uint32_t width, uint32_t height) {
    auto device = std::make_unique<DeviceVK>();
    if (!device->Initialize(windowHandle, width, height))
        return nullptr;
    return device;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
