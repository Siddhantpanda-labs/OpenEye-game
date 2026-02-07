#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/VulkanDeviceCreateInfo.h"
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

/** Create the Vulkan RHI device. Pass VulkanDeviceCreateInfo filled by the platform (instance, device, queues, optional surface). */
std::unique_ptr<IDevice> CreateVulkanDevice(const VulkanDeviceCreateInfo& info);

} // namespace rhi
} // namespace render
} // namespace eyelib
