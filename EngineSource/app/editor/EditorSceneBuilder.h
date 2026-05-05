#pragma once

#include "EditorContext.h"
#include "api/render/RenderDevice.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openeye::editor {

/**
 * Builds api::RenderScene from World::RenderSnapshot using editor camera and assets.
 * API-only; no animation/skinning. Used for scene view viewport.
 * When non-null, outDrawableToEntity lists entity ids in scene.opaqueStorage order; outTransparentDrawableToEntity lists ids in scene.transparentStorage order (for GPU picking).
 */
class EditorSceneBuilder {
public:
    /** Load materials (and meshes) referenced by the world into the registry so Build() can resolve materialAssetId. Call after LoadScene. */
    static void BindSceneAssets(const eyelib::api::world::World& world,
                                const std::vector<std::string>& assetRoots,
                                eyelib::api::AssetRegistry& registry);

    /** \a world reserved for callers; skybox comes from \a snapshot.skyboxAssetId (filled by CaptureRenderSnapshot). \a defaultMaterial when non-null is used for primitives/untextured meshes (avoids lazy creation in Build). */
    void Build(eyelib::api::RenderScene& scene,
               const eyelib::api::world::World::RenderSnapshot& snapshot,
               const EditorCamera& editorCamera,
               uint32_t viewportWidth,
               uint32_t viewportHeight,
               const std::vector<std::string>& assetRoots,
               eyelib::api::AssetRegistry* assetRegistry,
               std::shared_ptr<eyelib::api::Material> defaultMaterial = nullptr,
               const eyelib::api::world::World* world = nullptr,
               std::vector<::eyelib::world::EntityId>* outDrawableToEntity = nullptr,
               std::vector<::eyelib::world::EntityId>* outTransparentDrawableToEntity = nullptr);

    /** Load skybox cubemap from scene asset id (e.g. "Skybox/Citadella"). Returns null if missing or failed. Device required for GPU upload. */
    static std::shared_ptr<eyelib::api::TextureCube> LoadSkyboxFromAssetId(const std::string& assetId,
                                                                           const std::vector<std::string>& assetRoots,
                                                                           eyelib::api::RenderDevice device);

    /** Resolve material for scene (registry used only for GetId; GPU resolution via AssetManager). Use instead of registry->GetMaterial in Build. */
    static std::shared_ptr<eyelib::api::Material> ResolveMaterialForScene(eyelib::api::AssetRegistry* registry,
                                                                          const std::string& pathOrId);

private:
    struct ModelEntry {
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
    };
    struct DrawableCacheKey {
        ::eyelib::world::EntityId entity;
        uint8_t source = 0; // 0: Renderable, 1: MeshRenderer
        bool operator==(const DrawableCacheKey& other) const {
            return entity == other.entity && source == other.source;
        }
    };
    struct DrawableCacheKeyHash {
        size_t operator()(const DrawableCacheKey& k) const {
            const size_t h = ::eyelib::world::EntityIdHash{}(k.entity);
            return h ^ (static_cast<size_t>(k.source) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
        }
    };
    struct CachedDrawableStatic {
        std::string meshId;
        std::string materialAssetId;
        int32_t submeshIndex = -1;
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
        bool valid = false;
        uint64_t lastTouchedFrame = 0;
    };

    /** Resolve asset ref (path or GUID) to full path. If \a registry is non-null and path lookup fails, tries GUID via registry->GetFilePath. */
    static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots, eyelib::api::AssetRegistry* registry = nullptr);
    std::string ResolveAssetPathCached(const std::string& id, const std::vector<std::string>& roots, eyelib::api::AssetRegistry* registry = nullptr);
    std::shared_ptr<eyelib::api::Material> ResolveMaterialCached(eyelib::api::AssetRegistry* registry, const std::string& pathOrId);
    ModelEntry LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path);
    /** Resolve builtin:// primitive mesh id to procedural mesh + default material. Returns empty entry if not builtin. */
    ModelEntry GetPrimitiveEntry(const std::string& meshId);
    void EnsureDefaultMaterial();
    void RebuildDrawableStaticIfNeeded(const DrawableCacheKey& key,
                                       const std::string& meshId,
                                       const std::string& materialAssetId,
                                       int32_t submeshIndex,
                                       const std::vector<std::string>& assetRoots,
                                       eyelib::api::AssetRegistry* assetRegistry);
    void PruneStaleDrawableCache();

    std::unordered_map<std::string, ModelEntry> m_primitiveCache;
    std::unordered_map<std::string, std::shared_ptr<eyelib::api::TextureCube>> m_skyboxCache;
    std::unordered_map<std::string, std::string> m_resolvedAssetPathCache;
    std::unordered_map<std::string, std::weak_ptr<eyelib::api::Material>> m_materialLookupCache;
    std::unordered_map<DrawableCacheKey, CachedDrawableStatic, DrawableCacheKeyHash> m_drawableStaticCache;
    std::string m_assetRootsSignature;
    uint64_t m_buildFrameCounter = 0;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    eyelib::api::RenderDevice m_renderDevice;
};

} // namespace openeye::editor
