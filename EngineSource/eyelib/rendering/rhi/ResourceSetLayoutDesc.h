#pragma once

#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace rhi {

/** CPU-side descriptor layout for one resource-space set.
 *  Must match VulkanResourceSpaces + descriptor_binding_table.md.
 *  Shared by Vulkan and OpenGL backends. */
PipelineLayoutDesc MakeResourceSetDesc(IDevice::ResourceSetType type);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
