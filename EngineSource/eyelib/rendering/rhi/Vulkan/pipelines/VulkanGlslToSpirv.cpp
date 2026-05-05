#include "VulkanGlslToSpirv.h"

#include "eyelib/core/Log.h"

#if defined(OPENEYE_VULKAN_SHADERC)
#include <shaderc/shaderc.hpp>
#endif
#include <string>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {
const char* ShaderStageLabel(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex:
        return "vertex";
    case ShaderStage::Fragment:
        return "fragment";
    case ShaderStage::Compute:
        return "compute";
    case ShaderStage::Task:
        return "task";
    case ShaderStage::Mesh:
        return "mesh";
    default:
        return "unknown";
    }
}
} // namespace

bool VulkanCompileGlslToSpirv(const char* source, size_t sourceLen, ShaderStage stage, const char* inputFileName,
                              std::vector<uint32_t>& outSpirv, const char* logicalShaderName) {
    outSpirv.clear();
#if !defined(OPENEYE_VULKAN_SHADERC)
    (void)source;
    (void)sourceLen;
    (void)stage;
    (void)inputFileName;
    (void)logicalShaderName;
    core::Log::Error("VulkanCompileGlslToSpirv: build without OPENEYE_VULKAN_SHADERC (link shaderc from Vulkan SDK)");
    return false;
#else
    const char* fname = (inputFileName && inputFileName[0]) ? inputFileName : "shader.glsl";
    const char* logical =
        (logicalShaderName && logicalShaderName[0]) ? logicalShaderName : ((inputFileName && inputFileName[0]) ? inputFileName : "<unnamed>");
    if (!source || sourceLen == 0) {
        core::Log::ErrorF("VulkanCompileGlslToSpirv: empty source (logical='%s' stage=%s input='%s')", logical,
                          ShaderStageLabel(stage), fname);
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
    case ShaderStage::Task:
        kind = shaderc_glsl_task_shader;
        break;
    case ShaderStage::Mesh:
        kind = shaderc_glsl_mesh_shader;
        break;
    default:
        core::Log::ErrorF("VulkanCompileGlslToSpirv: unsupported stage %u (logical='%s' input='%s')",
                          static_cast<unsigned>(static_cast<uint32_t>(stage)), logical, fname);
        return false;
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    shaderc::SpvCompilationResult result =
        compiler.CompileGlslToSpv(source, sourceLen, kind, fname, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        const std::string& full = result.GetErrorMessage();
        std::string firstLine = full;
        const size_t nl = full.find('\n');
        if (nl != std::string::npos)
            firstLine = full.substr(0, nl);
        core::Log::ErrorF(
            "VulkanCompileGlslToSpirv: compile failed logical='%s' stage=%s input='%s' (first line): %s", logical,
            ShaderStageLabel(stage), fname, firstLine.c_str());
        if (nl != std::string::npos && full.size() > firstLine.size() + 1)
            core::Log::InfoF("VulkanCompileGlslToSpirv: full shaderc message:\n%s", full.c_str());
        return false;
    }
    outSpirv.assign(result.cbegin(), result.cend());
    return !outSpirv.empty();
#endif
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
