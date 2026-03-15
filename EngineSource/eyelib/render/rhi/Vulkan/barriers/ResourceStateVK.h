#pragma once

#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Vulkan barrier state: use RHI ResourceState (Undefined, ShaderResource, RenderTarget, TransferSrc/Dst, etc.). */
using ResourceStateVK = ResourceState;

} // namespace rhi
} // namespace render
} // namespace eyelib
