#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {
namespace rhi {

/** Vulkan barrier state: use RHI ResourceState (Undefined, ShaderResource, RenderTarget, TransferSrc/Dst, etc.). */
using ResourceStateVK = ResourceState;

} // namespace rhi
} // namespace rendering
} // namespace eyelib
