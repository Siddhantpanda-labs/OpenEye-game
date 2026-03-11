#pragma once

#include "EditorContext.h"
#include "api/render/RenderDevice.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::editor {

/**
 * Builds api::RenderScene from World::RenderSnapshot using editor camera and assets.
 * API-only; no animation/skinning. Used for scene view viewport.
 * When non-null, outDrawableToEntity is filled with entity id per drawable index (same order as scene.opaqueStorage) for outline/selection.
 */
class EditorSceneBuilder {
public:
    /** Load materials (and meshes) referenced by the world into the registry so Build() can resolve materialAssetId. Call after LoadScene. */
    static void BindSceneAssets(const eyelib::api::world::World& world,
                                const std::vector<std::string>& assetRoots,
                                eyelib::api::AssetRegistry& registry);

    /** \a world used for scene-level skybox (world->GetSkyboxAssetId()). Can be null. \a defaultMaterial when non-null is used for primitives/untextured meshes (avoids lazy creation in Build). */
    void Build(eyelib::api::RenderScene& scene,
               const eyelib::api::world::World::RenderSnapshot& snapshot,
               const EditorCamera& editorCamera,
               uint32_t viewportWidth,
               uint32_t viewportHeight,
               const std::vector<std::string>& assetRoots,
               eyelib::api::AssetRegistry* assetRegistry,
               std::shared_ptr<eyelib::api::Material> defaultMaterial = nullptr,
               const eyelib::api::world::World* world = nullptr,
               std::vector<::eyelib::world::EntityId>* outDrawableToEntity = nullptr);

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
    /** Resolve asset ref (path or GUID) to full path. If \a registry is non-null and path lookup fails, tries GUID via registry->GetFilePath. */
    static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots, eyelib::api::AssetRegistry* registry = nullptr);
    ModelEntry LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path);
    /** Resolve builtin:// primitive mesh id to procedural mesh + default material. Returns empty entry if not builtin. */
    ModelEntry GetPrimitiveEntry(const std::string& meshId);
    void EnsureDefaultMaterial();

    std::unordered_map<std::string, ModelEntry> m_modelCache;
    std::unordered_map<std::string, ModelEntry> m_primitiveCache;
    std::unordered_map<std::string, std::shared_ptr<eyelib::api::TextureCube>> m_skyboxCache;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    eyelib::api::RenderDevice m_renderDevice;
};

} // namespace openeye::editor
