#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"

#include <cstddef>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Compile GLSL to SPIR-V with **shaderc** (Vulkan 1.3 target). **`inputFileName`** is used for diagnostics in **`shaderc`** error messages.
 *  Returns false if **`OPENEYE_VULKAN_SHADERC`** is unset, source is empty, stage unsupported, or compilation fails (**errors logged**). */
bool VulkanCompileGlslToSpirv(const char* source, size_t sourceLen, ShaderStage stage, const char* inputFileName,
                              std::vector<uint32_t>& outSpirv);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
