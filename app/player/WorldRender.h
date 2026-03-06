#pragma once

#include "PlayerConfig.h"
#include "BuiltinResourceManager.h"
#include "RendererGPUResourceCache.h"

#include "api/assets/All.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"
#include "api/world/All.h"
#include "eyelib/vr/IXRProvider.h"
#include "eyelib/vr/XRViewHelpers.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace openeye::player {

class WorldAnimationBridge;

/** Use World::CaptureRenderSnapshot(snapshot) once per frame, then BuildScene(snapshot, ...). Snapshot type is eyelib::api::world::World::RenderSnapshot. */

class WorldRenderBridge {
public:
    /** Standalone: create pipeline and device from window (player app). */
    bool Initialize(eyelib::api::Window& window, uint32_t width, uint32_t height,
                    eyelib::render::RenderBackend backend = eyelib::render::RenderBackend::Default);
    /** Editor Model A: use shared device (same as editor). No second device; resources stay in same lifetime domain. */
    bool Initialize(void* sharedDevice, uint32_t width, uint32_t height);
    void Shutdown();

    /** Full render path (BuildScene + pipeline). Use only when render runs on same thread as game. */
    void Render(const RuntimeOptions& options, eyelib::api::world::World& world,
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
    void BuildScene(eyelib::api::world::World& world, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

    /** Game-thread only: fill scene from a snapshot (observation). Observation may be a view over a frame snapshot. */
    void BuildScene(const eyelib::api::world::Observation& obs, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

    /** Game-thread only: fill scene from the frame snapshot. One capture per frame (World::CaptureRenderSnapshot), then call this. */
    void BuildScene(const eyelib::api::world::World::RenderSnapshot& snapshot, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr);

    /** Native texture handle of pipeline output (after RenderSceneOnly). For ImGui use as ImTextureID; null if not ready. */
    void* GetOutputColorTextureNativeHandle() const;

    /** Optional XR provider. When set and session active, Render() uses stereo XR path when scene has XR camera. */
    void SetXRProvider(::eyelib::vr::IXRProvider* provider) { m_xrProvider = provider; }

    /** Device for asset loading / SetRenderDevice. Null if not initialized. */
    eyelib::api::RenderDevice GetDevice() const;

    /** Create default and skinned materials once after Initialize(). Call before BindAssets/scene build so primitives are visible. No-op if already created or headless. */
    void InitializeDefaultResources();

    /** Reload pipeline shaders (e.g. after ShaderWatcher copy). */
    void ReloadShaders();

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
    /** Resolve asset id to GPU material (renderer-owned resolution; scene building uses this instead of registry). */
    std::shared_ptr<eyelib::api::Material> ResolveMaterial(eyelib::api::AssetId id);
    /** Resolve material by path using registry only for GetId, then ResolveMaterial(id). */
    std::shared_ptr<eyelib::api::Material> ResolveMaterial(const std::string& materialId,
                                                           eyelib::api::AssetRegistry* registry);
    ModelEntry LoadModelEntry(const std::string& path);
    /** Resolve builtin:// primitive mesh id to procedural mesh + default material. */
    ModelEntry GetPrimitiveEntry(const std::string& meshId);
    std::shared_ptr<eyelib::api::TextureCube> GetOrLoadSkybox(const std::string& assetId,
                                                             const std::vector<std::string>& assetRoots);

    void BuildSceneFromViews(
        std::span<const eyelib::api::world::World::RenderableEntry> renderables,
        std::span<const eyelib::api::world::World::MeshRendererEntry> meshRenderers,
        std::span<const eyelib::api::world::World::CameraEntry> cameras,
        std::span<const eyelib::api::world::World::LightEntry> lights,
        std::function<std::optional<::eyelib::world::components::Transform>(::eyelib::world::EntityId)> getTransform,
        const RuntimeOptions& options,
        const WorldAnimationBridge* animationBridge,
        eyelib::api::AssetRegistry* assetRegistry,
        eyelib::api::RenderScene& scene,
        api::FrameAllocator* frameAlloc);

    eyelib::api::Renderer m_renderer;
    eyelib::api::RenderPipeline m_pipeline;
    bool m_ready = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::unique_ptr<BuiltinResourceManager> m_builtinManager;
    std::unique_ptr<RendererGPUResourceCache> m_gpuResourceCache;
    std::shared_ptr<eyelib::api::Shader> m_defaultShader;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::Shader> m_skinnedShader;
    std::vector<SkinnedMaterialEntry> m_skinnedMaterials;
    std::unordered_map<std::string, ModelEntry> m_modelCache;
    std::unordered_map<std::string, std::shared_ptr<eyelib::api::TextureCube>> m_skyboxCache;
    bool m_captureCallbackSet = false;
    eyelib::api::world::World::RenderSnapshot m_frameSnapshot;

    ::eyelib::vr::IXRProvider* m_xrProvider = nullptr;
    std::unique_ptr<eyelib::api::RenderTarget> m_xrEyeTargets[2];
    uint32_t m_xrEyeSize[2][2] = {{0, 0}, {0, 0}};
};

} // namespace openeye::player
