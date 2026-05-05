#pragma once

namespace eyelib {
namespace rendering {
namespace rhi {

/** Thin wrapper over VkPhysicalDevice. Selects a GPU from the instance: first discrete or integrated in
 *  enumeration order, else the first enumerated device. Logs name, type, and reported API version at selection. */
struct PhysicalDeviceVK {
    void* handle = nullptr;  // VkPhysicalDevice

    /** Enumerate physical devices and select one (prefer discrete/integrated GPU). Returns true if a device was selected. */
    static bool Select(PhysicalDeviceVK* out, void* instance);
    /** No-op (physical device is not destroyed). */
    static void Destroy(PhysicalDeviceVK* pd);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
