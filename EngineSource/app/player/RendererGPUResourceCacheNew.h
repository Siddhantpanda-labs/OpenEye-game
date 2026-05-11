#pragma once

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/rendering/Material.h"
#include "api/rendering/Mesh.h"
#include "api/rendering/Texture2D.h"

#include <memory>
#include <unordered_set>

namespace eyelib::api::rendering {
class ResourceCache;
}

namespace openeye::player {

/**
 * Runtime-shared resource resolver vending `eyelib::api::rendering::Mesh / Material /
 * Texture2D` handles via `eyelib::api::rendering::ResourceCache`.
 *
 * **Hosted in `OpenEyeGameRuntime`; consumed by both clients:**
 *  - **Player** (`WorldRenderBridge`, M4.3) — used to fill new-API scene draws during the
 *    legacy/new dual-stack and after the M4.4 device flip.
 *  - **Editor** (`EditorSceneBuilderNew`, M3.7) — same resolver fronting the editor's
 *    `ResourceCache` so scene-view + game-view panels get real `meshHandle` / `materialId`
 *    tokens instead of placeholder zeros.
 *
 * Sources data from `eyelib::api::AssetRegistry`'s legacy descriptor catalog
 * (`GetMeshDescriptor / GetTextureDescriptor / GetMaterialDescriptor`) and translates
 * those descriptors into the public POD types (`MeshData / TextureData / MaterialRecord`)
 * before handing them to the new cache. This is the only legal point where legacy
 * descriptor types touch the new API surface — kept inside `OpenEyeGameRuntime` so the
 * editor / player executables themselves stay free of the legacy descriptor includes.
 *
 * **Lifetime + threading.** Owned by the host bridge (`WorldRenderBridge` for the player;
 * `EditorApp` stash for the editor). Constructed with a borrowed pointer to a
 * `ResourceCache` (the cache itself owns GPU buffers and the new module resource
 * lifetimes). The legacy `RendererGPUResourceCache` is the one wired into
 * `AssetRegistry::SetGPUResolver` for the legacy stack — this resolver is queried
 * directly by the new-API scene builders, so it does not need to plug into
 * `IGPUResourceResolver` (whose surface vends legacy types only). Not thread-safe;
 * game-thread / main-thread only.
 *
 * **Cold-path posture.** When the underlying `ResourceCache` was constructed without a
 * live device (`ResourceCache::IsValid() == false`), `Create*` calls inside the cache
 * already warn-once and return invalid handles. This adapter passes through that
 * behaviour — `Get*` simply returns invalid handles to the caller.
 */
class RendererGPUResourceCacheNew {
public:
    RendererGPUResourceCacheNew(eyelib::api::rendering::ResourceCache* cache,
                                eyelib::api::AssetRegistry* registry) noexcept;
    ~RendererGPUResourceCacheNew();

    RendererGPUResourceCacheNew(const RendererGPUResourceCacheNew&)            = delete;
    RendererGPUResourceCacheNew& operator=(const RendererGPUResourceCacheNew&) = delete;

    /** Resolve mesh asset. Returns invalid handle on registry miss / cache cold-path. */
    eyelib::api::rendering::Mesh GetMesh(const eyelib::api::AssetId& id);
    /** Resolve texture asset. Returns invalid handle on registry miss / cache cold-path. */
    eyelib::api::rendering::Texture2D GetTexture(const eyelib::api::AssetId& id);
    /** Resolve material asset (and its referenced textures). Returns invalid handle on miss. */
    eyelib::api::rendering::Material GetMaterial(const eyelib::api::AssetId& id);

    /** Drop our resolution memo (the underlying cache also drops GPU resources). */
    void OnDeviceLost();
    /** Forget that `id` was resolved so the next `GetMaterial` re-creates it. */
    void InvalidateMaterial(const eyelib::api::AssetId& id);

private:
    /** Hash for `eyelib::api::AssetId` matching the `(high, low)` layout. */
    struct ApiAssetIdHash {
        size_t operator()(const eyelib::api::AssetId& id) const noexcept {
            return id.high ^ (id.low << 1);
        }
    };

    eyelib::api::rendering::ResourceCache* m_cache    = nullptr;
    eyelib::api::AssetRegistry*            m_registry = nullptr;

    /** Set of asset ids the adapter has already requested through the cache. The cache
     *  itself tracks the GPU lifetime; this set short-circuits redundant descriptor
     *  fetches (registry → POD translation is non-trivial for materials). */
    std::unordered_set<eyelib::api::AssetId, ApiAssetIdHash> m_meshesResolved;
    std::unordered_set<eyelib::api::AssetId, ApiAssetIdHash> m_texturesResolved;
    std::unordered_set<eyelib::api::AssetId, ApiAssetIdHash> m_materialsResolved;
};

} // namespace openeye::player
