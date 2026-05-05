#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {
namespace rhi {

/** Vulkan backend uses the portable `ResourceState` enum (`RHITypes.h`) for barrier planning.
 *  `utils/VulkanEnums.h` maps states to `VkImageLayout`, `VkAccessFlags`, and `VkPipelineStageFlags`. */
using ResourceStateVK = ResourceState;

} // namespace rhi
} // namespace rendering
} // namespace eyelib
