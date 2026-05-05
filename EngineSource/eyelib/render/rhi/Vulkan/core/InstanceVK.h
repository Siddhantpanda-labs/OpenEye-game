#pragma once

#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/** Thin wrapper over VkInstance. Handles creation with standard app/engine info and surface extensions. */
struct InstanceVK {
    void* handle = nullptr;  // VkInstance
    uint32_t apiVersion = 0; // Negotiated VkInstance API version

    /** Create instance with OpenEye app/engine info and VK_KHR_surface + platform surface extension. Returns true on success. */
    static bool Create(InstanceVK* out,
                      const char* appName = "OpenEye",
                      uint32_t appVersion = 0,
                      const char* engineName = "OpenEye",
                      uint32_t engineVersion = 0,
                      uint32_t apiVersion = 0 /* auto-negotiate */);
    /** Destroy instance. */
    static void Destroy(InstanceVK* inst);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
