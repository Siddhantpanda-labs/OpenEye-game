#include "eyelib/rendering/rhi/Vulkan/memory/AllocatorVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include "eyelib/rendering/rhi/Vulkan/memory/vk_mem_alloc.h"

namespace eyelib {
namespace rendering {
namespace rhi {

bool AllocatorVK::Create(AllocatorVK* out, void* physicalDevice, void* device, void* instance, uint32_t vulkanApiVersion,
                         bool enableBufferDeviceAddress) {
    if (!out || !physicalDevice || !device || !instance) return false;
    VmaAllocatorCreateInfo info = {};
    info.physicalDevice = static_cast<VkPhysicalDevice>(physicalDevice);
    info.device = static_cast<VkDevice>(device);
    info.instance = static_cast<VkInstance>(instance);
    info.vulkanApiVersion = vulkanApiVersion;
    if (enableBufferDeviceAddress)
        info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkResult res = vmaCreateAllocator(&info, &allocator);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("AllocatorVK::Create: vmaCreateAllocator failed (0x%x)", res);
        return false;
    }
    out->handle = allocator;
    core::Log::InfoF("Vulkan: VMA allocator created (Vulkan API version %u.%u.%u for allocator).",
                     VK_VERSION_MAJOR(vulkanApiVersion),
                     VK_VERSION_MINOR(vulkanApiVersion),
                     VK_VERSION_PATCH(vulkanApiVersion));
    return true;
}

void AllocatorVK::Destroy(AllocatorVK* a) {
    if (!a || !a->handle) return;
    vmaDestroyAllocator(static_cast<VmaAllocator>(a->handle));
    a->handle = nullptr;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
