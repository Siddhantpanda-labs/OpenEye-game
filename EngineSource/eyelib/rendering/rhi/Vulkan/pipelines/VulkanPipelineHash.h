#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Hash PipelineDesc for graphics pipeline cache lookup. */
uint64_t HashPipelineDesc(const PipelineDesc& desc);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
