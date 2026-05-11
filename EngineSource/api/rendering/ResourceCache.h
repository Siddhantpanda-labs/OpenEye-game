#pragma once

#include "api/asset/AssetId.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/MeshData.h"
#include "api/rendering/RenderDevice.h"
#include "api/rendering/TextureData.h"

// Public PIMPL handle headers (forward-declare the resource types we vend).
namespace eyelib {
namespace api {
namespace rendering {
class Material;
class Mesh;
class RenderPipeline;
class Texture2D;
}}}

#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Resource cache that vends `api/rendering` GPU handles by `AssetId`.
 *
 * Replaces the editor's legacy `EditorGPUResourceCache` (and the player's
 * `RendererGPUResourceCache` once M4.3 lands). Owns:
 *
 *   - **Mesh entries** (`AssetId → eyelib::rendering::Mesh + 32-bit token`). Created
 *     via `CreateMesh(id, MeshData)`. Implements `eyelib::rendering::IMeshHandleResolver`
 *     internally so attached pipelines can fetch GPU buffers by token at draw-time.
 *   - **Texture entries** (`AssetId → eyelib::rendering::Texture2D + 32-bit token`).
 *     Created via `CreateTexture(id, TextureData)`. Tokens are written into
 *     `MaterialRecord::*Texture` slots; resolution to a sampler binding happens once
 *     the new module's texture binding-table feature lands (token contract is captured
 *     today so `MaterialRecord` rows stay forward-compatible).
 *   - **Material entries** (`AssetId → MaterialRecord + 32-bit materialId`). Created
 *     via `CreateMaterial(id, MaterialRecord)`. Pushed into every attached pipeline's
 *     `RenderResources::MaterialTable` immediately, and re-pushed whenever the editor
 *     mutates the record through a `Material` PIMPL setter.
 *
 * **Pipeline attachment.** Editor / player call `pipeline.AttachResourceCache(*cache)`
 * once after the pipeline is initialized. Attaching primes the pipeline's MaterialTable
 * with all currently-known records; subsequent `CreateMaterial` calls fan out to every
 * attached pipeline. The same cache may be attached to multiple pipelines (editor
 * scene-view + game-view + material-preview share assets).
 *
 * **Threading.** Not thread-safe. The editor / player calls these methods from the
 * UI / scene-build thread, the same way they called the legacy cache. Pipelines must
 * also be attached / detached from that thread.
 *
 * **Sealed-module contract.** This is the public façade — it never names
 * `eyelib::rendering::*` types. The `.cpp` is the only place that translates between
 * the public POD descriptors (`MeshData`, `TextureData`, `MaterialRecord`) and the
 * new module's internal resource APIs.
 */
class ResourceCache {
public:
    /**
     * @param device  Opaque new-API device handle (typically `RenderPipeline::GetDevice()`
     *                after the device has been created). Must be non-null and resolve to
     *                a valid `eyelib::rendering::rhi::IDevice*`; `Create*` calls fail
     *                gracefully (return invalid handles, log once) when the device is null
     *                so the cache type itself can be constructed before the device exists
     *                (cold-path during M3 dual-stack window).
     */
    explicit ResourceCache(RenderDevice device) noexcept;
    ~ResourceCache();

    ResourceCache(const ResourceCache&)            = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&)                 = delete;
    ResourceCache& operator=(ResourceCache&&)      = delete;

    /** True when constructed with a real (non-null) device. False during the M3 cold-path
     *  window; in that state every `Create*` returns an invalid handle and warns once. */
    bool IsValid() const noexcept;

    // ===== Creation =====================================================================
    //
    // Each `Create*` is also an "upsert": passing an `AssetId` that already has an entry
    // replaces the old entry's contents (and re-publishes material rows to attached
    // pipelines). The handle token stays stable across replace so existing draw lists do
    // not need to be rewired.

    /** Upload `data` to the device and register it under `id`. Returns an invalid handle
     *  on device-null or upload failure. */
    Mesh CreateMesh(const eyelib::api::AssetId& id, const MeshData& data);

    /** Load the file referenced by `data` and register it under `id`. Returns an invalid
     *  handle on device-null or load failure. */
    Texture2D CreateTexture(const eyelib::api::AssetId& id, const TextureData& data);

    /** Register `record` under `id` and push it into every attached pipeline's MaterialTable.
     *  The returned handle's setters mutate this record in-place + re-publish. */
    Material CreateMaterial(const eyelib::api::AssetId& id, const MaterialRecord& record);

    // ===== Lookup ======================================================================

    /** Returns a fresh handle wrapping the cached entry, or an invalid handle when `id`
     *  has not been registered. Lookups are O(1) amortized (hash table). */
    Mesh      GetMesh(const eyelib::api::AssetId& id);
    Texture2D GetTexture(const eyelib::api::AssetId& id);
    Material  GetMaterial(const eyelib::api::AssetId& id);

    /** Cheap presence checks — useful for the editor scene builder to decide whether to
     *  resolve a snapshot id through the cache or fall back to a placeholder. */
    bool HasMesh(const eyelib::api::AssetId& id) const;
    bool HasTexture(const eyelib::api::AssetId& id) const;
    bool HasMaterial(const eyelib::api::AssetId& id) const;

    // ===== Lifecycle ===================================================================

    /** Drop the entry for `id` across all three resource families (mesh / texture /
     *  material). Material rows stay in attached MaterialTables (slot is still addressable
     *  by id) until the next `CreateMaterial(sameId, ...)` overwrites the row. */
    void Remove(const eyelib::api::AssetId& id);

    /** Drop all cached entries (e.g. on Vulkan `VK_ERROR_DEVICE_LOST`). Attached pipelines
     *  remain attached; their MaterialTables retain their last uploaded rows. */
    void OnDeviceLost();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    // The pipeline / material PIMPL internals reach into the cache through the
    // friend-only access seam declared in `internal/ResourceAccess.h`.
    friend class ResourceCacheAccess;
};

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
