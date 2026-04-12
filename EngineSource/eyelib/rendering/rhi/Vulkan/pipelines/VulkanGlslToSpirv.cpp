#include "VulkanGlslToSpirv.h"

#include "eyelib/core/Log.h"

#if defined(OPENEYE_VULKAN_SHADERC)
#include <shaderc/shaderc.hpp>
#endif

namespace eyelib {
namespace rendering {
namespace rhi {

bool VulkanCompileGlslToSpirv(const char* source, size_t sourceLen, ShaderStage stage, const char* inputFileName,
                              std::vector<uint32_t>& outSpirv) {
    outSpirv.clear();
#if !defined(OPENEYE_VULKAN_SHADERC)
    (void)source;
    (void)sourceLen;
    (void)stage;
    (void)inputFileName;
    core::Log::Error("VulkanCompileGlslToSpirv: build without OPENEYE_VULKAN_SHADERC (link shaderc from Vulkan SDK)");
    return false;
#else
    if (!source || sourceLen == 0) {
        core::Log::Error("VulkanCompileGlslToSpirv: empty source");
        return false;
    }
    shaderc_shader_kind kind = shaderc_glsl_vertex_shader;
    switch (stage) {
    case ShaderStage::Vertex:
        kind = shaderc_glsl_vertex_shader;
        break;
    case ShaderStage::Fragment:
        kind = shaderc_glsl_fragment_shader;
        break;
    case ShaderStage::Compute:
        kind = shaderc_glsl_compute_shader;
        break;
    default:
        core::Log::ErrorF("VulkanCompileGlslToSpirv: unsupported stage %u", static_cast<unsigned>(static_cast<uint32_t>(stage)));
        return false;
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    const char* fname = (inputFileName && inputFileName[0]) ? inputFileName : "shader.glsl";
    shaderc::SpvCompilationResult result =
        compiler.CompileGlslToSpv(source, sourceLen, kind, fname, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        core::Log::ErrorF("shaderc: %s", result.GetErrorMessage().c_str());
        return false;
    }
    outSpirv.assign(result.cbegin(), result.cend());
    return !outSpirv.empty();
#endif
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
