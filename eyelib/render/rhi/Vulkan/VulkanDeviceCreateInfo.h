#pragma once

#include <vulkan/vulkan_core.h>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/** Parameters to create the Vulkan RHI device. Filled by the platform layer after creating instance/device/surface. */
struct VulkanDeviceCreateInfo {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    /** Optional. If valid, swapchain will be created for Present/BlitToDefaultFramebuffer. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    /** If surface is valid and present queue differs from graphics, set these. */
    uint32_t presentQueueFamilyIndex = 0;
    VkQueue presentQueue = VK_NULL_HANDLE;
    /** Minimum image count for swapchain (e.g. 2 for double buffering). */
    uint32_t minImageCount = 2;
    /** Enable validation layers and debug reporting. */
    bool enableValidation = false;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
