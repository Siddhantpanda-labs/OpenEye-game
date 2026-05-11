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
 * M4.3 — Player-side parallel resource resolver vending `eyelib::api::rendering::Mesh /
 * Material / Texture2D` handles via `eyelib::api::rendering::ResourceCache`.
 *
 * Cold-path peer of `RendererGPUResourceCache` (legacy) for the duration of M4.
 * Sources data from `eyelib::api::AssetRegistry`'s legacy descriptor catalog
 * (`GetMeshDescriptor / GetTextureDescriptor / GetMaterialDescriptor`) and translates
 * those descriptors into the public POD types (`MeshData / TextureData / MaterialRecord`)
 * before handing them to the new cache.
 *
 * **Lifetime + threading.** Owned by `WorldRenderBridge`. Constructed with a borrowed
 * pointer to a `ResourceCache` (the cache itself owns GPU buffers and the new module
 * resource lifetimes). The legacy `RendererGPUResourceCache` is the one wired into
 * `AssetRegistry::SetGPUResolver` for the legacy stack — this `*New` adapter is queried
 * directly by the new-API scene builder once non-null, so it does not need to plug into
 * `IGPUResourceResolver` (whose surface vends legacy types only).
 *
 * **Cold-path posture.** When the underlying `ResourceCache` was constructed without a
 * live device (`ResourceCache::IsValid() == false`), `Create*` calls inside the cache
 * already warn-once and return invalid handles. This adapter passes through that
 * behaviour — `Get*` simply returns invalid handles to the caller, and the parallel
 * scene builder keeps placeholder zero tokens. The runtime flip happens at M4.4 when the
 * new `RenderPipeline` is initialized and `m_resourceCacheNew` is rebuilt against its
 * device.
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
