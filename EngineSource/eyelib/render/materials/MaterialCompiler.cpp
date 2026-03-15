#include "eyelib/render/materials/MaterialCompiler.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {

namespace {

std::string getTemplateNameForStage(PipelineStage stage, InstanceType instanceType) {
    switch (stage) {
    case PipelineStage::GBuffer:
        if (instanceType == InstanceType::Instanced) return "passes/deferred/gbuffer_instanced";
        return "passes/deferred/gbuffer_skinned";
    case PipelineStage::Depth:
        if (instanceType == InstanceType::Instanced) return "passes/shadow/shadow_instanced";
        return (instanceType == InstanceType::Skinned) ? "passes/shadow/shadow_skinned" : "passes/shadow/shadow";
    case PipelineStage::Forward:
        if (instanceType == InstanceType::Instanced) return {};  // no forward_instanced template; use ForwardPlus for instanced
        return (instanceType == InstanceType::Skinned) ? "passes/forward/forward_skinned" : "passes/forward/forward";
    case PipelineStage::ForwardPlus:
        if (instanceType == InstanceType::Instanced) return "passes/forward/forwardplus_instanced";
        return (instanceType == InstanceType::Skinned) ? "passes/forward/forwardplus_skinned" : "passes/forward/forwardplus";
    default:
        return {};
    }
}

std::string getMaterialDefines(const Material* material) {
    if (!material) return {};
    std::string out;
    switch (material->GetShadingModel()) {
    case ShadingModel::Lit:   out += "#define SHADING_LIT 1\n"; break;
    case ShadingModel::Unlit: out += "#define SHADING_UNLIT 1\n"; break;
    case ShadingModel::Custom: out += "#define SHADING_CUSTOM 1\n"; break;
    }
    if (material->GetTexture("u_AlbedoTexture")) out += "#define HAS_ALBEDO_TEXTURE\n";
    if (material->GetTexture("u_NormalTexture")) out += "#define HAS_NORMAL_MAP\n";
    if (material->GetTexture("u_MetallicRoughnessTexture")) out += "#define HAS_METALLIC_ROUGHNESS_TEXTURE\n";
    if (material->GetTexture("u_MetallicTexture")) out += "#define HAS_METALLIC_TEXTURE\n";
    if (material->GetTexture("u_RoughnessTexture")) out += "#define HAS_ROUGHNESS_TEXTURE\n";
    if (material->GetTexture("u_AOTexture")) out += "#define HAS_AO_TEXTURE\n";
    if (material->GetTexture("u_EmissiveTexture")) out += "#define HAS_EMISSIVE_TEXTURE\n";
    if (material->GetRefraction()) out += "#define HAS_REFRACTION\n";
    if (material->GetTwoSided()) out += "#define HAS_TWO_SIDED\n";
    return out;
}

void replacePlaceholder(std::string& src, const char* placeholder, const std::string& value) {
    const size_t plen = std::strlen(placeholder);
    size_t pos = 0;
    for (;;) {
        pos = src.find(placeholder, pos);
        if (pos == std::string::npos) break;
        src.replace(pos, plen, value);
        pos += value.size();
    }
}

} // namespace

std::shared_ptr<Shader> MaterialCompiler::compile(rhi::IDevice* device, const Material* material,
                                                  PipelineStage stage, InstanceType instanceType,
                                                  std::vector<std::string>* outDependencies) {
    if (!device || !material) return nullptr;

    const char* backend = device->GetShaderBackendName();
    const char* vertExt = device->GetShaderVertexExtension();
    const char* fragExt = device->GetShaderFragmentExtension();
    if (!backend || !vertExt || !fragExt) {
        core::Log::Error("MaterialCompiler: device missing shader path info");
        return nullptr;
    }

    std::string logicalName = getTemplateNameForStage(stage, instanceType);
    if (logicalName.empty()) return nullptr;

    std::string root = platform::filesystem::GetEngineAssetsRoot();
    std::string basePath = root.empty()
        ? (std::string("assets/shaders/") + backend + "/" + logicalName)
        : platform::filesystem::NormalizePath(root + "/assets/shaders/" + backend + "/" + logicalName);
    std::string vertexPath = basePath + vertExt;
    std::string fragmentPath = basePath + fragExt;

    std::string vertexSrc, fragmentSrc;
    std::vector<std::string> vertDeps, fragDeps;
    bool okVert = outDependencies
        ? LoadShaderSourceWithDependencies(vertexPath, vertexSrc, &vertDeps)
        : LoadShaderSource(vertexPath, vertexSrc);
    bool okFrag = outDependencies
        ? LoadShaderSourceWithDependencies(fragmentPath, fragmentSrc, &fragDeps)
        : LoadShaderSource(fragmentPath, fragmentSrc);
    if (!okVert || !okFrag) {
        core::Log::ErrorF("MaterialCompiler: failed to load template %s", logicalName.c_str());
        return nullptr;
    }
    if (outDependencies) {
        *outDependencies = std::move(vertDeps);
        for (const std::string& p : fragDeps) {
            if (std::find(outDependencies->begin(), outDependencies->end(), p) == outDependencies->end())
                outDependencies->push_back(p);
        }
    }

    std::string defines = getMaterialDefines(material);
    replacePlaceholder(fragmentSrc, "@MATERIAL_DEFINES@", defines);
    replacePlaceholder(fragmentSrc, "@MATERIAL_FRAGMENT_SNIPPET@", material->GetFragmentSnippet());
    replacePlaceholder(fragmentSrc, "@MATERIAL_SHADING_SNIPPET@", material->GetShadingSnippet());

    rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::Legacy;
    if (stage == PipelineStage::Depth) layoutKind = rhi::PipelineLayoutKind::Shadow;
    if (stage == PipelineStage::GBuffer) layoutKind = rhi::PipelineLayoutKind::GBuffer;
    if (stage == PipelineStage::Forward || stage == PipelineStage::ForwardPlus)
        layoutKind = rhi::PipelineLayoutKind::Forward;

    auto shader = std::make_shared<Shader>();
    if (!shader->BuildFromSource(device, vertexSrc, fragmentSrc, Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, layoutKind)) {
        core::Log::ErrorF("MaterialCompiler: BuildFromSource failed for %s (materialTypeId=%llu)", logicalName.c_str(), static_cast<unsigned long long>(material->GetShaderIndex()));
        return nullptr;
    }
    return shader;
}

} // namespace render
} // namespace eyelib
