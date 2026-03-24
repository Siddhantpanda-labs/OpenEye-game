#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** CPU-side descriptor layout for one resource-space set.
 *  Must match VulkanResourceSpaces + descriptor_binding_table.md.
 *  Shared by Vulkan and OpenGL backends. */
PipelineLayoutDesc MakeResourceSetDesc(IDevice::ResourceSetType type);

} // namespace rhi
} // namespace render
} // namespace eyelib
