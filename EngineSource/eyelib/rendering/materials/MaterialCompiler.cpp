#include "eyelib/rendering/materials/MaterialCompiler.h"
#include "eyelib/rendering/materials/Material.h"
#include "eyelib/rendering/resources/Shader.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/shaders/ShaderSourceLoader.h"
#include "eyelib/rendering/shaders/ShaderTemplateName.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>
#include <cstring>
#include <optional>

namespace eyelib {
namespace rendering {

namespace {

std::string getTemplateNameForStage(PipelineStage stage, InstanceType instanceType) {
    // Material templates keep legacy static->gbuffer_skinned behavior.
    return ResolveShaderTemplateName(stage, instanceType, false);
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

    rhi::IShader* vs = nullptr;
    std::string vertexSrc, fragmentSrc;
    std::vector<std::string> vertDeps, fragDeps;

    const bool isVulkan = (std::strcmp(backend, "vulkan") == 0);
    if (isVulkan) {
        const std::string vertSpvPath = basePath + ".vert.spv";
        std::vector<uint8_t> vertSpv = platform::filesystem::ReadFile(vertSpvPath);
        const uint32_t spirvMagic = 0x07230203u;
        if (vertSpv.size() >= 4 && *reinterpret_cast<uint32_t*>(vertSpv.data()) == spirvMagic) {
            vs = device->CreateShaderFromBinary(rhi::ShaderStage::Vertex, vertSpv.data(), vertSpv.size());
            if (vs && vs->IsValid()) {
                /* vertex from precompiled .spv */
            } else {
                if (vs) { device->DestroyShader(vs); vs = nullptr; }
            }
        }
    }
    if (!vs) {
        bool okVert = outDependencies
            ? LoadShaderSourceWithDependencies(vertexPath, vertexSrc, &vertDeps)
            : LoadShaderSource(vertexPath, vertexSrc);
        if (!okVert) {
            core::Log::ErrorF("MaterialCompiler: failed to load vertex template %s", logicalName.c_str());
            return nullptr;
        }
        vs = device->CreateShader(rhi::ShaderStage::Vertex, vertexSrc.c_str(), vertexSrc.size());
        if (!vs || !vs->IsValid()) {
            if (vs) device->DestroyShader(vs);
            core::Log::ErrorF("MaterialCompiler: failed to create vertex shader for %s", logicalName.c_str());
            return nullptr;
        }
    }

    bool okFrag = outDependencies
        ? LoadShaderSourceWithDependencies(fragmentPath, fragmentSrc, &fragDeps)
        : LoadShaderSource(fragmentPath, fragmentSrc);
    if (!okFrag) {
        if (vs) device->DestroyShader(vs);
        core::Log::ErrorF("MaterialCompiler: failed to load fragment template %s", logicalName.c_str());
        return nullptr;
    }
    if (outDependencies) {
        outDependencies->clear();
        if (!vertDeps.empty())
            *outDependencies = std::move(vertDeps);
        else if (isVulkan)
            outDependencies->push_back(basePath + ".vert.spv");
        for (const std::string& p : fragDeps) {
            if (std::find(outDependencies->begin(), outDependencies->end(), p) == outDependencies->end())
                outDependencies->push_back(p);
        }
    }

    std::string defines = getMaterialDefines(material);
    replacePlaceholder(fragmentSrc, "@MATERIAL_DEFINES@", defines);
    replacePlaceholder(fragmentSrc, "@MATERIAL_FRAGMENT_SNIPPET@", material->GetFragmentSnippet());
    replacePlaceholder(fragmentSrc, "@MATERIAL_SHADING_SNIPPET@", material->GetShadingSnippet());

    rhi::IShader* fs = device->CreateShader(rhi::ShaderStage::Fragment, fragmentSrc.c_str(), fragmentSrc.size());
    if (!fs || !fs->IsValid()) {
        if (vs) device->DestroyShader(vs);
        if (fs) device->DestroyShader(fs);
        core::Log::ErrorF("MaterialCompiler: failed to create fragment shader for %s", logicalName.c_str());
        return nullptr;
    }

    rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly;
    if (stage == PipelineStage::Depth) layoutKind = rhi::PipelineLayoutKind::Shadow;
    if (stage == PipelineStage::GBuffer) layoutKind = rhi::PipelineLayoutKind::GBuffer;
    if (stage == PipelineStage::Forward || stage == PipelineStage::ForwardPlus)
        layoutKind = rhi::PipelineLayoutKind::Forward;

    std::optional<rhi::PipelineAttachmentFormats> attachmentFormats;
    switch (stage) {
    case PipelineStage::GBuffer:
        attachmentFormats = rhi::StandardAttachmentFormats::GBufferMRT();
        break;
    case PipelineStage::Depth:
        attachmentFormats = rhi::StandardAttachmentFormats::ShadowDepthOnly();
        break;
    case PipelineStage::Forward:
    case PipelineStage::ForwardPlus:
        attachmentFormats = rhi::StandardAttachmentFormats::ForwardColorDepth();
        break;
    default:
        break;
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->BuildFromShaders(device, vs, fs, Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList,
                                  layoutKind, attachmentFormats)) {
        if (vs) device->DestroyShader(vs);
        if (fs) device->DestroyShader(fs);
        core::Log::ErrorF("MaterialCompiler: BuildFromShaders failed for %s (materialTypeId=%llu)", logicalName.c_str(), static_cast<unsigned long long>(material->GetShaderIndex()));
        return nullptr;
    }
    return shader;
}

} // namespace rendering
} // namespace eyelib
