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
 */
class EditorSceneBuilder {
public:
    void Build(eyelib::api::RenderScene& scene,
               const eyelib::api::world::World::RenderSnapshot& snapshot,
               const EditorCamera& editorCamera,
               uint32_t viewportWidth,
               uint32_t viewportHeight,
               const std::vector<std::string>& assetRoots,
               eyelib::api::AssetRegistry* assetRegistry);

private:
    struct ModelEntry {
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
    };
    static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots);
    ModelEntry LoadModelEntry(const std::string& path);
    void EnsureDefaultMaterial();
    void EnsureDefaultSkybox(const std::vector<std::string>& assetRoots);

    std::unordered_map<std::string, ModelEntry> m_modelCache;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::TextureCube> m_defaultSkybox;
    bool m_defaultSkyboxAttempted = false;
};

} // namespace openeye::editor
