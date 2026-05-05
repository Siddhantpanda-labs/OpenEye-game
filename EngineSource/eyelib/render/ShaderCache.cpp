#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/materials/MaterialCompiler.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/ShaderTemplateName.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>

namespace eyelib {
namespace render {

namespace {

std::string getShaderNameForKey(const ShaderKey& key) {
    return ResolveShaderTemplateName(key.stage, key.instanceType, true);
}

bool isKeySupported(const ShaderKey& key) {
    switch (key.stage) {
    case PipelineStage::GBuffer:
    case PipelineStage::Depth:
    case PipelineStage::Forward:
    case PipelineStage::ForwardPlus:
        return true;
    default:
        return false;
    }
}

} // namespace

namespace {
/** Default material for default shader key: Lit, no textures. Used so template placeholders are replaced. */
const Material* getDefaultMaterial() {
    static Material s_default;
    return &s_default;
}
} // namespace

std::shared_ptr<Shader> ShaderCache::getOrCompile(rhi::IDevice* device, const ShaderKey& key) {
    if (!device || !isKeySupported(key)) return nullptr;
    return getOrCompile(device, key, getDefaultMaterial());
}

static int64_t getMaxFileTime(const std::vector<std::string>& paths) {
    int64_t t = 0;
    for (const std::string& p : paths) {
        int64_t ft = platform::filesystem::GetFileTime(p);
        if (ft > t) t = ft;
    }
    return t;
}

static bool anyFileNewerThan(const std::vector<std::string>& paths, int64_t timestamp) {
    for (const std::string& p : paths) {
        if (platform::filesystem::GetFileTime(p) > timestamp) return true;
    }
    return false;
}

static std::vector<std::string> getDefaultShaderDependencies(const ShaderKey& key, rhi::IDevice* device) {
    std::vector<std::string> deps;
    if (!device) return deps;
    const std::string logical = getShaderNameForKey(key);
    if (logical.empty()) return deps;
    const char* backend = device->GetShaderBackendName();
    const char* vertExt = device->GetShaderVertexExtension();
    const char* fragExt = device->GetShaderFragmentExtension();
    if (!backend || !vertExt || !fragExt) return deps;
    std::string root = platform::filesystem::GetEngineAssetsRoot();
    const std::string basePath = root.empty()
        ? (std::string("assets/shaders/") + backend + "/" + logical)
        : platform::filesystem::NormalizePath(root + "/assets/shaders/" + backend + "/" + logical);
    deps.push_back(basePath + std::string(vertExt));
    deps.push_back(basePath + std::string(fragExt));
    deps.push_back(basePath + ".vert.spv");
    deps.push_back(basePath + ".frag.spv");
    return deps;
}

static std::shared_ptr<Shader> loadDefaultMaterialShader(rhi::IDevice* device, const ShaderKey& key) {
    if (!device) return nullptr;
    const std::string logical = getShaderNameForKey(key);
    if (logical.empty()) return nullptr;

    rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly;
    std::optional<rhi::PipelineAttachmentFormats> attachmentFormats;
    switch (key.stage) {
    case PipelineStage::GBuffer:
        layoutKind = rhi::PipelineLayoutKind::GBuffer;
        attachmentFormats = rhi::StandardAttachmentFormats::GBufferMRT();
        break;
    case PipelineStage::Depth:
        layoutKind = rhi::PipelineLayoutKind::Shadow;
        attachmentFormats = rhi::StandardAttachmentFormats::ShadowDepthOnly();
        break;
    case PipelineStage::Forward:
    case PipelineStage::ForwardPlus:
        layoutKind = rhi::PipelineLayoutKind::Forward;
        attachmentFormats = rhi::StandardAttachmentFormats::ForwardColorDepth();
        break;
    default:
        return nullptr;
    }

    auto shader = std::make_shared<Shader>();
    if (!shader->Load(device, logical, Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList,
                      layoutKind, attachmentFormats)) {
        return nullptr;
    }
    return shader;
}

std::shared_ptr<Shader> ShaderCache::getOrCompile(rhi::IDevice* device, const ShaderKey& key, const Material* material) {
    if (!device) return nullptr;

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        ShaderCacheEntry& e = it->second;
        if (e.shader && e.shader->IsValid()) {
            if (!e.dependencyPaths.empty() && anyFileNewerThan(e.dependencyPaths, e.compiledTimestamp)) {
                m_cache.erase(it);
            } else {
                return e.shader;
            }
        } else {
            m_cache.erase(it);
        }
    }
    if (!material) return nullptr;

    const bool isDefaultMaterialKey = (key.materialTypeId == kDefaultMaterialTypeId) && (material == getDefaultMaterial());
    if (isDefaultMaterialKey) {
        auto shader = loadDefaultMaterialShader(device, key);
        if (!shader) return nullptr;
        ShaderCacheEntry entry;
        entry.shader = shader;
        entry.dependencyPaths = getDefaultShaderDependencies(key, device);
        entry.compiledTimestamp = getMaxFileTime(entry.dependencyPaths);
        m_cache[key] = std::move(entry);
        return shader;
    }

    std::vector<std::string> deps;
    MaterialCompiler compiler;
    auto shader = compiler.compile(device, material, key.stage, key.instanceType, &deps);
    if (!shader) return nullptr;

    ShaderCacheEntry entry;
    entry.shader = shader;
    entry.dependencyPaths = std::move(deps);
    entry.compiledTimestamp = getMaxFileTime(entry.dependencyPaths);
    m_cache[key] = std::move(entry);
    return shader;
}

void ShaderCache::clear() {
    m_cache.clear();
}

bool ShaderCache::reloadAll() {
    m_cache.clear();
    return true;
}

} // namespace render
} // namespace eyelib
