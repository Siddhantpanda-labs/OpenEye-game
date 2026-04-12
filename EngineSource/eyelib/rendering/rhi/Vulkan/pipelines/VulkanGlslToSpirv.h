#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"

#include <cstddef>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Compile GLSL to SPIR-V for Vulkan (shaderc). Returns false if OPENEYE_VULKAN_SHADERC is unset or compile fails. */
bool VulkanCompileGlslToSpirv(const char* source, size_t sourceLen, ShaderStage stage, const char* inputFileName,
                              std::vector<uint32_t>& outSpirv);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
