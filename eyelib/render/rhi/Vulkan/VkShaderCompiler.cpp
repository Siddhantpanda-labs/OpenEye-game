#include "eyelib/render/rhi/Vulkan/VkShaderCompiler.h"
#include "eyelib/core/Log.h"
#include <cstring>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

std::vector<uint32_t> CompileGLSLToSPIRV(ShaderStage stage, const char* source, size_t sourceLen) {
    std::vector<uint32_t> spirv;
#if defined(OPENEYE_USE_GLSLANG)
    (void)stage;
    (void)source;
    (void)sourceLen;
    // When glslang is linked: use glslang::Compile to produce SPIR-V.
    // For now we compile nothing and return empty (shaders must be precompiled or use glslang).
    core::Log::WarnF("VkShaderCompiler: GLSL->SPIR-V requires glslang (OPENEYE_USE_GLSLANG). Precompile shaders to .spv or add glslang.");
    return spirv;
#else
    (void)stage;
    (void)source;
    (void)sourceLen;
    core::Log::WarnF("VkShaderCompiler: GLSL->SPIR-V not available. Use precompiled .spv or enable glslang.");
    return spirv;
#endif
}

} // namespace rhi
} // namespace render
} // namespace eyelib
