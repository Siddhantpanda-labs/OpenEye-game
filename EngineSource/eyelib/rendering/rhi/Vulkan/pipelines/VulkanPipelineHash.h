#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Hash **`PipelineDesc`** for **`DeviceVK`** graphics pipeline cache. Includes shader **pointer identity** (recreated **`IShader`** → new hash),
 *  fixed-function state, **`layoutKind`**, and optional **`attachmentFormats`** (explicit vs omitted branches differ — matches dynamic rendering create path). */
uint64_t HashPipelineDesc(const PipelineDesc& desc);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
