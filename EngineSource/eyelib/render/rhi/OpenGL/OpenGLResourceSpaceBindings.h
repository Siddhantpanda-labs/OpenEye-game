#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Map logical descriptor binding (per ResourceSetType) to OpenGL binding points.
 *  Fragment stage: layout(binding=N) must be unique across UBO, SSBO, and texture samplers.
 *  DeferredLightingPass offsets textures (logical 0–4 -> physical 2–6), SSBO 5 -> 7, UBO 6 -> 8, IBL 7–9 -> 9–11 so Frame UBO can use 0–1.
 *  Must match shaders under assets/shaders/opengl/ that use resource-space layouts. */
uint32_t GLBindingForResourceSet(IDevice::ResourceSetType set, BindingType type, uint32_t logicalBinding);

} // namespace rhi
} // namespace render
} // namespace eyelib
