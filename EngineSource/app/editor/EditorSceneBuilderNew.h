#pragma once

#include "EditorContext.h"
#include "api/rendering/RenderScene.h"
#include "api/world/All.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib { namespace api { class AssetRegistry; } }
namespace eyelib { namespace api { namespace rendering { class ResourceCache; } } }

namespace openeye::editor {

/**
 * Editor scene builder targeting the new public API (`api/rendering`).
 *
 * Mirrors the legacy `EditorSceneBuilder` shape (snapshot in, scene out, optional entity-id
 * passthrough) but emits `eyelib::api::rendering::RenderScene` instead of the legacy
 * `eyelib::api::RenderScene`. This is the M2.3 deliverable: the data plumbing on the new API
 * is wired and compiles end-to-end.
 *
 * **Resource resolution gap (intentional, tracked).** v1 leaves
 * `DrawableInstance::meshHandle` and `materialId` at the zero (invalid) token — the editor's
 * GPU resource cache still vends legacy `eyelib::api::Mesh/Material` shared-pointers and has
 * no path to the new façade's opaque tokens until M3.7 (`EditorGPUResourceCache` migration).
 * Camera, world transform, draw-bucket routing, instance flags, and per-entity passthrough
 * arrays are all populated for real, so the M3 closeout edit only has to wire up the cache
 * lookup — every other contract is already shaped here.
 *
 * **Lights and skybox are not in `eyelib::api::rendering::RenderScene` v1** (see
 * `RenderScene.h` docstring). The new module's lighting bus reaches the editor through the
 * façade's lifecycle hooks, not the per-frame scene POD; this builder's surface stays
 * snapshot-in-only and does not iterate `snapshot.lights` / `snapshot.skyboxAssetId`.
 *
 * **Hard-constraint discipline.** This translation unit is the *only* editor file that may
 * include `api/rendering/...`. It must never include `eyelib/rendering/...` directly (the
 * façade exists precisely to preserve that boundary).
 */
class EditorSceneBuilderNew {
public:
    /**
     * Build the new POD scene from world snapshot + editor camera. Parallels the legacy
     * `EditorSceneBuilder::Build` shape (entity-id passthrough arrays included for M2.5
     * picking parity), but without the legacy resource caches — see class docstring.
     */
    void Build(eyelib::api::rendering::RenderScene& scene,
               const eyelib::api::world::World::RenderSnapshot& snapshot,
               const EditorCamera& editorCamera,
               uint32_t viewportWidth,
               uint32_t viewportHeight,
               const std::vector<std::string>& assetRoots,
               eyelib::api::AssetRegistry* assetRegistry,
               std::vector<::eyelib::world::EntityId>* outDrawableToEntity = nullptr,
               std::vector<::eyelib::world::EntityId>* outTransparentDrawableToEntity = nullptr,
               // M3.7 — optional new-API resource cache. When non-null, the builder
               // resolves snapshot meshAssetId / materialAssetId to real
               // `eyelib::api::rendering::Mesh::GetMeshHandle()` / `Material::GetMaterialId()`
               // tokens; otherwise `meshHandle` / `materialId` stay 0 (cold path).
               eyelib::api::rendering::ResourceCache* resourceCacheNew = nullptr);
};

} // namespace openeye::editor
