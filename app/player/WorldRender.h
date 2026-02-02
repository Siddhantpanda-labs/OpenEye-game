#pragma once

#include "PlayerConfig.h"

#include "api/assets/All.h"
#include "api/render/All.h"
#include "api/world/All.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace openeye::player {

class WorldAnimationBridge;

class WorldRenderBridge {
public:
    bool Initialize(uint32_t width, uint32_t height);
    void Shutdown();
    void Render(const RuntimeOptions& options, const eyelib::api::world::World& world,
                const WorldAnimationBridge* animationBridge,
                eyelib::api::AssetRegistry* assetRegistry,
                uint32_t width, uint32_t height);

private:
    struct ModelEntry {
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
        std::shared_ptr<eyelib::api::Skeleton> skeleton;
        std::vector<eyelib::math::Mat4> bindPoseMatrices;
    };
    struct SkinnedMaterialEntry {
        ::eyelib::world::EntityId entity;
        std::shared_ptr<eyelib::api::Material> baseMaterial;
        std::shared_ptr<eyelib::api::MaterialInstance> instance;
    };

    void EnsureDefaultMaterial();
    void EnsureSkinnedMaterial();
    std::shared_ptr<eyelib::api::MaterialInstance> GetSkinnedMaterialInstance(
        ::eyelib::world::EntityId entity,
        const std::shared_ptr<eyelib::api::Material>& baseMaterial);
    std::shared_ptr<eyelib::api::Material> ResolveMaterial(const std::string& materialId,
                                                           eyelib::api::AssetRegistry* registry);
    ModelEntry LoadModelEntry(const std::string& path);
    void BuildScene(const eyelib::api::world::World& world, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene);

    eyelib::api::Renderer m_renderer;
    eyelib::api::RenderPipeline m_pipeline;
    bool m_ready = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::shared_ptr<eyelib::api::Shader> m_defaultShader;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::Shader> m_skinnedShader;
    std::vector<SkinnedMaterialEntry> m_skinnedMaterials;
    std::unordered_map<std::string, ModelEntry> m_modelCache;
};

} // namespace openeye::player
