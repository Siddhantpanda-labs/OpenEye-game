#pragma once

#include "EditorContext.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"

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

    void Build(eyelib::api::RenderScene& scene,
               const eyelib::api::world::World::RenderSnapshot& snapshot,
               const EditorCamera& editorCamera,
               uint32_t viewportWidth,
               uint32_t viewportHeight,
               const std::vector<std::string>& assetRoots,
               eyelib::api::AssetRegistry* assetRegistry,
               std::vector<::eyelib::world::EntityId>* outDrawableToEntity = nullptr);

private:
    struct ModelEntry {
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
    };
    static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots);  // used by BindSceneAssets and Build
    ModelEntry LoadModelEntry(const std::string& path);
    void EnsureDefaultMaterial();
    void EnsureDefaultSkybox(const std::vector<std::string>& assetRoots);

    std::unordered_map<std::string, ModelEntry> m_modelCache;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::TextureCube> m_defaultSkybox;
    bool m_defaultSkyboxAttempted = false;
};

} // namespace openeye::editor
