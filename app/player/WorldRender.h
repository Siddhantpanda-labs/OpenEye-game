#pragma once

#include "PlayerConfig.h"

#include "api/assets/All.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"
#include "api/world/All.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::player {

class WorldAnimationBridge;

/** Immutable copy of world state needed to build a render scene. Build from World once per frame, then call BuildScene(snapshot, ...) so the render path never reads live world. */
struct RenderSnapshot {
    std::vector<eyelib::api::world::World::RenderableEntry> renderables;
    std::vector<eyelib::api::world::World::MeshRendererEntry> meshRenderers;
    std::vector<eyelib::api::world::World::CameraEntry> cameras;
    std::vector<eyelib::api::world::World::LightEntry> lights;
    std::unordered_map<eyelib::world::EntityId, eyelib::world::components::Transform, eyelib::world::EntityIdHash> transformCache;

    std::optional<eyelib::world::components::Transform> GetTransform(eyelib::world::EntityId entity) const;
};

/** Game-thread only: capture a snapshot of the world for rendering. Use with BuildScene(snapshot, ...). */
RenderSnapshot CreateRenderSnapshot(const eyelib::api::world::World& world);

class WorldRenderBridge {
public:
    /** Single init path: window + size + backend (e.g. from --renderer). */
    bool Initialize(eyelib::api::Window& window, uint32_t width, uint32_t height,
                    eyelib::render::RenderBackend backend = eyelib::render::RenderBackend::Default);
    void Shutdown();

    /** Full render path (BuildScene + pipeline). Use only when render runs on same thread as game. */
    void Render(const RuntimeOptions& options, const eyelib::api::world::World& world,
                const WorldAnimationBridge* animationBridge,
                eyelib::api::AssetRegistry* assetRegistry,
                uint32_t width, uint32_t height);

    /** Render-thread only: apply a Resize (Phase 5). Must be called at frame boundary, before RenderSceneOnly for this frame. */
    void ApplyResize(uint32_t width, uint32_t height);

    /** Render-thread only: Resize (if needed), Render(scene), BlitToScreen. No World/animation access. Resize only runs when size changed. */
    bool RenderSceneOnly(const eyelib::api::RenderScene& scene, uint32_t width, uint32_t height);

    /** Render-thread only: wait for GPU to finish. Call before SwapBuffers to reduce flicker (test fix 1). */
    void Finish();

    /** Render-thread only: insert GPU fence after current command stream. Call before Finish(). Returns opaque fence (null if unsupported). */
    void* InsertFrameFence();
    /** Render-thread only: wait for fence (no destroy). Call before SwapBuffers so present happens after GPU completes. */
    void WaitFence(void* fence);
    /** Render-thread only: wait for fence then destroy. Call after SwapBuffers before ReleaseFrame to ensure GPU finished with slot. */
    void WaitAndDestroyFence(void* fence);

    /** Game-thread only: fill scene from world + animation. Used with RenderFrameSync when using a render thread. */
    void BuildScene(const eyelib::api::world::World& world, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

    /** Game-thread only: fill scene from a snapshot (observation). Use this to avoid reading live world during build. */
    void BuildScene(const eyelib::api::world::Observation& obs, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

    /** Game-thread only: fill scene from a formal snapshot. Preferred when using a render thread: CreateRenderSnapshot(world) then BuildScene(snapshot, ...). */
    void BuildScene(const RenderSnapshot& snapshot, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

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
