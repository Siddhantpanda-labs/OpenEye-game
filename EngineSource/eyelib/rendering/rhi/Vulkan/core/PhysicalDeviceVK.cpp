#include "eyelib/rendering/rhi/Vulkan/core/PhysicalDeviceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

bool PhysicalDeviceVK::Select(PhysicalDeviceVK* out, void* instance) {
    if (!out || !instance) return false;
    VkInstance inst = static_cast<VkInstance>(instance);
    uint32_t count = 0;
    VkResult res = vkEnumeratePhysicalDevices(inst, &count, nullptr);
    if (res != VK_SUCCESS || count == 0) {
        core::Log::ErrorF("PhysicalDeviceVK::Select: vkEnumeratePhysicalDevices failed (0x%x)", res);
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    res = vkEnumeratePhysicalDevices(inst, &count, devices.data());
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("PhysicalDeviceVK::Select: vkEnumeratePhysicalDevices (get) failed (0x%x)", res);
        return false;
    }
    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (VkPhysicalDevice pd : devices) {
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            chosen = pd;
            break;
        }
    }
    if (chosen == VK_NULL_HANDLE)
        chosen = devices[0];
    out->handle = chosen;
    return (chosen != VK_NULL_HANDLE);
}

void PhysicalDeviceVK::Destroy(PhysicalDeviceVK* pd) {
    if (!pd) return;
    pd->handle = nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
