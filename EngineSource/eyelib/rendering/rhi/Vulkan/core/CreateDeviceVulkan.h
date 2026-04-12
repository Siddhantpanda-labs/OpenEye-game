#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Create Vulkan RHI device. windowHandle = native window (e.g. HWND), width/height for swapchain. Returns nullptr if Vulkan init fails. */
std::unique_ptr<IDevice> CreateVulkanDevice(void* windowHandle, uint32_t width, uint32_t height);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
