#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/materials/MaterialCompiler.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHI.h"
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
        return key.materialTypeId == kDefaultMaterialTypeId;
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

std::shared_ptr<Shader> ShaderCache::getOrCompile(rhi::IDevice* device, const ShaderKey& key, const Material* material) {
    if (!device) return nullptr;

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        if (it->second && it->second->IsValid()) return it->second;
        m_cache.erase(it);
    }
    if (!material) return nullptr;

    MaterialCompiler compiler;
    auto shader = compiler.compile(device, material, key.stage, key.instanceType);
    if (!shader) return nullptr;
    m_cache[key] = shader;
    return shader;
}

void ShaderCache::clear() {
    m_cache.clear();
}

bool ShaderCache::reloadAll() {
    // Cache entries are built via MaterialCompiler (template + material); they have no load path
    // for Reload(). Clear the cache so next getOrCompile() recompiles from current template files.
    m_cache.clear();
    return true;
}

} // namespace render
} // namespace eyelib
