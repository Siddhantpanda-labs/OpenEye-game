#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Map logical descriptor binding (per ResourceSetType) to OpenGL binding points.
 *  UBO, SSBO, and texture units are separate GL namespaces (same index allowed across kinds).
 *  Must match shaders under assets/shaders/opengl/ that use resource-space layouts. */
uint32_t GLBindingForResourceSet(IDevice::ResourceSetType set, BindingType type, uint32_t logicalBinding);

} // namespace rhi
} // namespace render
} // namespace eyelib
