#pragma once

#include "api/asset/AssetId.h"
#include "api/rendering/Material.h"
#include "api/rendering/Mesh.h"

#include <string>
#include <unordered_map>

namespace eyelib::api::rendering {
class ResourceCache;
}

namespace openeye::player {

/**
 * Runtime-shared builtin resource manager vending `eyelib::api::rendering::Mesh` +
 * `Material` for builtin primitive ids (`builtin://Cube`, `builtin://Sphere`, …) via
 * `ResourceCache`.
 *
 * **Hosted in `OpenEyeGameRuntime`; consumed by both clients:**
 *  - **Player** (`WorldRenderBridge`, M4.3) — primitive resolution during the dual-stack
 *    and after the M4.4 device flip.
 *  - **Editor** (`EditorSceneBuilderNew`, M3.7) — primitive resolution for scene-view +
 *    game-view panels so cubes / spheres / planes draw with the shared default material.
 *
 * **Default material.** Authored once via `ResourceCache::CreateMaterial` under a stable
 * synthetic asset id so every primitive shares the same row in the MaterialTable. Both
 * clients also call `GetDefaultMaterial()` as a fallback when an entity has no explicit
 * material assigned.
 *
 * **Threading.** Not thread-safe. Game-thread / main-thread only (matches the legacy
 * manager).
 */
class BuiltinResourceManagerNew {
public:
    struct PrimitiveEntry {
        eyelib::api::rendering::Mesh     mesh;
        eyelib::api::rendering::Material material;
    };

    explicit BuiltinResourceManagerNew(eyelib::api::rendering::ResourceCache* cache) noexcept;
    ~BuiltinResourceManagerNew();

    BuiltinResourceManagerNew(const BuiltinResourceManagerNew&)            = delete;
    BuiltinResourceManagerNew& operator=(const BuiltinResourceManagerNew&) = delete;

    /** Authored once. Subsequent calls return the cached handle. */
    eyelib::api::rendering::Material GetDefaultMaterial();

    /** Returns mesh + default material for builtin id (`builtin://Cube`, …). Returns
     *  empty entry on unknown id or cold-path cache. */
    PrimitiveEntry GetPrimitiveEntry(const std::string& meshId);

private:
    eyelib::api::rendering::ResourceCache* m_cache = nullptr;

    /** Stable synthetic AssetIds for the builtin primitives + default material. */
    eyelib::api::AssetId m_defaultMaterialAssetId;
    std::unordered_map<std::string, eyelib::api::AssetId> m_primitiveAssetIds;
    std::unordered_map<std::string, PrimitiveEntry>       m_primitiveCache;
};

}  // namespace openeye::player
