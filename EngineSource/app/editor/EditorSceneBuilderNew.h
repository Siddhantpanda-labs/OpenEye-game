#pragma once

#include "EditorContext.h"
#include "api/rendering/RenderScene.h"
#include "api/world/All.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib { namespace api { class AssetRegistry; } }
namespace openeye { namespace player {
class RendererGPUResourceCacheNew;
class BuiltinResourceManagerNew;
} }
namespace openeye::editor { class EditorPersistentSceneCache; }
namespace openeye::editor { class EditorVisibilitySystem; }

namespace openeye::editor {

/**
 * Editor scene builder targeting the new public API (`api/rendering`).
 *
 * Mirrors the legacy `EditorSceneBuilder` shape (snapshot in, scene out, optional entity-id
 * passthrough) but emits `eyelib::api::rendering::RenderScene` instead of the legacy
 * `eyelib::api::RenderScene`.
 *
 * **Resource resolution.** Mesh + material handles come from the runtime-shared resolver
 * (`openeye::player::RendererGPUResourceCacheNew`) + builtin-primitive vendor
 * (`openeye::player::BuiltinResourceManagerNew`) — the same two classes the player uses,
 * hosted in `OpenEyeGameRuntime`. Both are required for the builder to fill real tokens;
 * either being null falls back to zero handles (cold path). Built-in mesh ids
 * (`builtin://Cube`, …) resolve through the builtin manager; file assets go through the
 * resolver (legacy `AssetRegistry` descriptor → public POD → `ResourceCache::Create*` on
 * first reference). Materials missing a row default to `BuiltinResourceManagerNew::
 * GetDefaultMaterial()` so untextured drawables still render.
 *
 * **Lights** are pushed through `RenderScene` per-frame (directional / point / spot) — the
 * builder mirrors the player's `WorldRenderBridge` conversion so editor + player share one
 * light convention. Skybox is still on the legacy stack until per-scene IBL probe swap
 * lands (see `Render_steps_todo.md` Section C); `snapshot.skyboxAssetId` is intentionally
 * not consumed here.
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
     * picking parity). Pass the runtime-shared resolver + builtin manager (typically
     * `ctx.gpuResolverNew` / `ctx.builtinManagerNew`) to get real GPU handles; pass null
     * for the cold path. When `sceneCacheNew` is provided, handle resolution is cached
     * per drawable key (entity + asset ids + submesh + layer).
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
               openeye::player::RendererGPUResourceCacheNew* gpuResolverNew = nullptr,
               openeye::player::BuiltinResourceManagerNew*   builtinManagerNew = nullptr,
               openeye::editor::EditorPersistentSceneCache* sceneCacheNew = nullptr,
               openeye::editor::EditorVisibilitySystem* visibilitySystem = nullptr);

private:
    void BuildTransformLookup(const eyelib::api::world::World::RenderSnapshot& snapshot);

    // Step 4: persistent SoA transform lookup storage reused frame-to-frame.
    std::vector<uint32_t> m_transformSlotGeneration;
    std::vector<uint32_t> m_transformSlotIndex;
    std::vector<uint32_t> m_touchedTransformSlots;
};

} // namespace openeye::editor
