#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/materials/MaterialCompiler.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>

namespace eyelib {
namespace render {

namespace {

/** Resolve shader logical name for the given key. Phase 1: only GBuffer; same shader for Static/Skinned. */
std::string getShaderNameForKey(const ShaderKey& key) {
    switch (key.stage) {
    case PipelineStage::GBuffer:
        return "passes/deferred/gbuffer_skinned";
    case PipelineStage::Depth:
        return (key.instanceType == InstanceType::Skinned) ? "passes/shadow/shadow_skinned" : "passes/shadow/shadow";
    default:
        return {};
    }
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
