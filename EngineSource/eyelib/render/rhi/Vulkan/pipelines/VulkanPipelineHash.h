#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/** Hash PipelineDesc for graphics pipeline cache lookup. */
uint64_t HashPipelineDesc(const PipelineDesc& desc);

} // namespace rhi
} // namespace render
} // namespace eyelib
