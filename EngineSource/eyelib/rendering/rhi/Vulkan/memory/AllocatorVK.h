#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Thin wrapper over VMA (VkMemAlloc) allocator. */
struct AllocatorVK {
    void* handle = nullptr;  // VmaAllocator

    /**
     * Create VMA allocator. physicalDevice/device/instance are VkPhysicalDevice/VkDevice/VkInstance.
     * vulkanApiVersion e.g. VK_API_VERSION_1_2.
     */
    static bool Create(AllocatorVK* out, void* physicalDevice, void* device, void* instance, uint32_t vulkanApiVersion);
    static void Destroy(AllocatorVK* a);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
