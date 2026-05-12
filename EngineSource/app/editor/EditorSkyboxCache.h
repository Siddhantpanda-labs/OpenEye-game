#pragma once

#include "api/rendering/RenderDevice.h"
#include "api/rendering/Skybox.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace openeye::editor {

/**
 * Per-editor cache of `api::rendering::Skybox` instances keyed by world `skyboxAssetId`.
 *
 * Lifetime: owned by `EditorApp` inside the same scope as the editor pipelines and
 * `gpuResolverNew` / `builtinManagerNew` so the `Skybox` GPU resources (cubemap textures)
 * are released before the RHI device is torn down. The cache is exposed to panels via
 * a raw pointer on `EditorContext` (`skyboxCacheNew`).
 *
 * Resolution policy (mirrors the legacy player path in
 * `eyelib::render::LoadSkyboxCubemapFromAssetId`):
 *  1. Empty id or `eyelib::world::skybox::IsProceduralSkyboxId(id)` → returns a cached
 *     procedural-only `Skybox` (no GPU resources).
 *  2. Otherwise resolves the asset path against `assetRoots` + (optionally) `AssetRegistry`.
 *     If the resolved path is a directory, loads `posx.jpg` … `negz.jpg` as a six-face
 *     cubemap; otherwise treats it as a horizontal-cross image (legacy convention).
 *  3. On any failure the cache stores a "no sky" sentinel so the next frame's lookup is
 *     O(1) and does not re-attempt to load the same broken asset.
 *
 * Thread safety: not thread-safe. Call from the render thread only.
 */
class EditorSkyboxCache {
public:
    EditorSkyboxCache() = default;
    ~EditorSkyboxCache() = default;

    EditorSkyboxCache(const EditorSkyboxCache&) = delete;
    EditorSkyboxCache& operator=(const EditorSkyboxCache&) = delete;

    /**
     * Resolve the requested skybox. Returns a non-owning pointer that is valid until the
     * cache is destroyed. Pass the result straight into `RenderScene::skybox`. May return
     * nullptr only when `device` is null; otherwise a `Skybox` is always returned (kind
     * may be `Procedural` or `None` on failure paths).
     *
     * `crossFaceSize` is the per-face pixel size for horizontal-cross sources; pass
     * `eyelib::world::skybox::kDefaultCrossFaceSize` for the engine's default 512 px cross.
     */
    eyelib::api::rendering::Skybox* GetOrLoad(eyelib::api::rendering::RenderDevice device,
                                              const std::string& assetId,
                                              const std::vector<std::string>& assetRoots,
                                              eyelib::api::AssetRegistry* assetRegistry,
                                              uint32_t crossFaceSize);

    /** Drop all cached entries (frees GPU resources). Call before device shutdown. */
    void Clear() noexcept { m_cache.clear(); }

private:
    std::unordered_map<std::string, std::unique_ptr<eyelib::api::rendering::Skybox>> m_cache;
};

} // namespace openeye::editor
