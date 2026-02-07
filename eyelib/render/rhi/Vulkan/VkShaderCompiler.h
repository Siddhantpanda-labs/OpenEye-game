#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

/** Compile GLSL source to SPIR-V. Returns empty vector on failure. */
std::vector<uint32_t> CompileGLSLToSPIRV(ShaderStage stage, const char* source, size_t sourceLen);

} // namespace rhi
} // namespace render
} // namespace eyelib
