#pragma once

#include "eyelib/render/ShaderKey.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <cstdint>

namespace eyelib {
namespace render {

namespace rhi { struct IDevice; }
class Shader;
class Material;

/** Per-cache-entry metadata for shader hot-reload: dependency paths and compile-time timestamp. */
struct ShaderCacheEntry {
    std::shared_ptr<Shader> shader;
    std::vector<std::string> dependencyPaths;
    int64_t compiledTimestamp = 0;  // max GetFileTime of deps at compile time
};

/**
 * Cache of compiled shaders by ShaderKey.
 * Tracks shader file dependencies; invalidates and recompiles when any dependency is newer than compile time.
 */
class ShaderCache {
public:
    ShaderCache() = default;
    ~ShaderCache() = default;

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;

    /** Get or compile shader for the given key. Returns null if device is null or compile fails. Invalidates if any dependency file is newer. */
    std::shared_ptr<Shader> getOrCompile(rhi::IDevice* device, const ShaderKey& key);
    /** When key.materialTypeId != kDefaultMaterialTypeId, material is required on cache miss to compile variant. */
    std::shared_ptr<Shader> getOrCompile(rhi::IDevice* device, const ShaderKey& key, const Material* material);

    /** Invalidate all cached shaders (e.g. on device loss or reload). */
    void clear();

    /** Hot-reload: clear cache so next getOrCompile recompiles from current template files. */
    bool reloadAll();

private:
    std::map<ShaderKey, ShaderCacheEntry> m_cache;
};

} // namespace render
} // namespace eyelib
