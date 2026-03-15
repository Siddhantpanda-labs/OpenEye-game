#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

/** Create Vulkan RHI device. windowHandle = native window (e.g. HWND), width/height for swapchain. Returns nullptr if Vulkan init fails. */
std::unique_ptr<IDevice> CreateVulkanDevice(void* windowHandle, uint32_t width, uint32_t height);

} // namespace rhi
} // namespace render
} // namespace eyelib
