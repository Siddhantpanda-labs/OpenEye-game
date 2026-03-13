#pragma once

#include "eyelib/render/ShaderKey.h"
#include <memory>
#include <map>

namespace eyelib {
namespace render {

namespace rhi { struct IDevice; }
class Shader;
class Material;

/**
 * Cache of compiled shaders by ShaderKey.
 * Phase 1: getOrCompile loads the existing pass shader (e.g. gbuffer_skinned) per key.
 * Phase 3: when key.materialTypeId != default, getOrCompile(device, key, material) compiles via MaterialCompiler.
 */
class ShaderCache {
public:
    ShaderCache() = default;
    ~ShaderCache() = default;

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;

    /** Get or compile shader for the given key. Returns null if device is null or compile fails. */
    std::shared_ptr<Shader> getOrCompile(rhi::IDevice* device, const ShaderKey& key);
    /** When key.materialTypeId != kDefaultMaterialTypeId, material is required on cache miss to compile variant. */
    std::shared_ptr<Shader> getOrCompile(rhi::IDevice* device, const ShaderKey& key, const Material* material);

    /** Invalidate all cached shaders (e.g. on device loss or reload). */
    void clear();

    /** Hot-reload: clear cache so next getOrCompile recompiles from current template files. */
    bool reloadAll();

private:
    std::map<ShaderKey, std::shared_ptr<Shader>> m_cache;
};

} // namespace render
} // namespace eyelib
