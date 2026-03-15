#pragma once

namespace eyelib {
namespace render {
namespace rhi {

/** Thin wrapper over VkPhysicalDevice. Selects preferred GPU from instance (discrete/integrated, else first). */
struct PhysicalDeviceVK {
    void* handle = nullptr;  // VkPhysicalDevice

    /** Enumerate physical devices and select one (prefer discrete/integrated GPU). Returns true if a device was selected. */
    static bool Select(PhysicalDeviceVK* out, void* instance);
    /** No-op (physical device is not destroyed). */
    static void Destroy(PhysicalDeviceVK* pd);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
