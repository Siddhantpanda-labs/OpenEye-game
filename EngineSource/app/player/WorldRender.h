#pragma once

#include "PlayerConfig.h"
#include "BuiltinResourceManager.h"
#include "BuiltinResourceManagerNew.h"
#include "RendererGPUResourceCache.h"
#include "RendererGPUResourceCacheNew.h"
#include "SkyboxCacheNew.h"

#include "api/asset/All.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"
#include "api/rendering/Renderer.h"
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/ResourceCache.h"
#include "api/world/All.h"
#include "eyelib/vr/IXRProvider.h"
#include "eyelib/vr/XRViewHelpers.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eyelib::api::rendering {
struct RenderScene;
}

namespace openeye::player {

class WorldAnimationBridge;

/** Borrowed editor-owned caches for embedded Play on shared `api/rendering` device. */
struct WorldRenderBridgeExternalsNew {
    eyelib::api::rendering::ResourceCache* resourceCache = nullptr;
    RendererGPUResourceCacheNew*           gpuResolver = nullptr;
    BuiltinResourceManagerNew*             builtinManager = nullptr;
    SkyboxCacheNew*                        skyboxCache = nullptr;
};

/**
 * Per-frame render snapshot wiring:
 * - Call BindWorldSnapshotCapture(world) once after Initialize() when this bridge owns presentation with that world.
 * - Call UnbindWorldSnapshotCapture(world) before destroying this bridge (GameRuntime::Shutdown does both).
 * - Do not call World::SetCaptureSnapshotCallback from BuildScene or hosts; keeps Tick vs Render order safe.
 */
class WorldRenderBridge {
public:
    /** Standalone: create pipeline and device from window (player app). Pass registry so GPU resolver is set. */
    bool Initialize(eyelib::api::Window& window, uint32_t width, uint32_t height,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::render::RenderBackend backend = eyelib::render::RenderBackend::Default);
    /** Editor Model A: use shared device (same as editor). Pass registry so GPU resolver is set. */
    bool Initialize(void* sharedDevice, uint32_t width, uint32_t height, eyelib::api::AssetRegistry* assetRegistry);
    /** Editor: shared `api/rendering` device (embedded Play). Legacy stacks stay default-constructed. */
    bool Initialize(eyelib::api::rendering::RenderDevice sharedDevice, uint32_t width, uint32_t height,
                    eyelib::api::AssetRegistry* assetRegistry);
    /** Shared-device init that borrows editor-owned caches where provided. */
    bool Initialize(eyelib::api::rendering::RenderDevice sharedDevice, uint32_t width, uint32_t height,
                    eyelib::api::AssetRegistry* assetRegistry,
                    const WorldRenderBridgeExternalsNew& externals);
    void Shutdown();

    /**
     * Register ecs end-of-frame snapshot capture (scheduler callback) and seed first snapshot.
     * Pair with UnbindWorldSnapshotCapture before this bridge is destroyed.
     */
    void BindWorldSnapshotCapture(eyelib::api::world::World& world);
    /** Clear capture callback and frame snapshot pointer on \a world (safe if already unbound). */
    void UnbindWorldSnapshotCapture(eyelib::api::world::World& world);

    /**
     * Game-thread: FlushTransforms + CaptureRenderSnapshot into this bridge's buffer and SetFrameSnapshot on \a world.
     * Use when the host does not call World::Update (e.g. edit preview) so Render() matches the live ECS.
     */
    void CaptureRenderSnapshotNow(eyelib::api::world::World& world);

    /** Full render path (BuildScene + pipeline). Use only when render runs on same thread as game. */
    void Render(const RuntimeOptions& options, eyelib::api::world::World& world,
                const WorldAnimationBridge* animationBridge,
                eyelib::api::AssetRegistry* assetRegistry,
                uint32_t width, uint32_t height);

    /** Render-thread only: apply a Resize (Phase 5). Must be called at frame boundary, before RenderSceneOnly for this frame. */
    void ApplyResize(uint32_t width, uint32_t height);

    /** Render-thread only: Resize (if needed), Render(scene); when this bridge owns presentation it also blits/presents.
     *  Legacy-typed scene; when `kUseNewRenderPipeline` is on, this path warns once and is skipped — use
     *  `RenderSceneOnlyNew` (the new-API typed overload) instead. */
    bool RenderSceneOnly(const eyelib::api::RenderScene& scene, uint32_t width, uint32_t height);

    /** Render-thread only: Resize (if needed), Render(sceneNew); when this bridge owns presentation, blits/presents.
     *  M4.4 — only valid when `kUseNewRenderPipeline` is on (legacy path warns + no-ops). */
    bool RenderSceneOnlyNew(const eyelib::api::rendering::RenderScene& sceneNew, uint32_t width, uint32_t height);

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
                    api::FrameAllocator* frameAlloc = nullptr,
                    eyelib::api::rendering::RenderScene* parallelSceneOut = nullptr);

    /** Game-thread only: fill scene from a snapshot (observation). Observation may be a view over a frame snapshot. */
    void BuildScene(const eyelib::api::world::Observation& obs, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr,
                    eyelib::api::rendering::RenderScene* parallelSceneOut = nullptr);

    /** Game-thread only: fill scene from the frame snapshot. One capture per frame (World::CaptureRenderSnapshot), then call this. */
    void BuildScene(const eyelib::api::world::World::RenderSnapshot& snapshot, const RuntimeOptions& options,
                    const WorldAnimationBridge* animationBridge,
                    eyelib::api::AssetRegistry* assetRegistry,
                    eyelib::api::RenderScene& scene,
                    api::FrameAllocator* frameAlloc = nullptr,
                    eyelib::api::rendering::RenderScene* parallelSceneOut = nullptr);

    /** Native texture handle of pipeline output (after RenderSceneOnly). For ImGui use as ImTextureID; null if not ready. */
    void* GetOutputColorTextureNativeHandle() const;

    /** Read current frame output as RGBA8. Call after Render(); performs Finish() then readback. outRGBA must be at least width*height*4. Returns false if not ready or readback unsupported. */
    bool ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA);

    /** Streaming: prepare a 960x540 RGBA8 copy of the current frame (blit from pipeline output). Call before readback when clients are connected. */
    void PrepareStreamingFrame();
    /** Streaming (sync): read back the 960x540 frame into outRGBA. Returns false if streaming target not ready. Use when PBO async path is unavailable. */
    bool ReadStreamingPixels(uint8_t* outRGBA);
    /** Streaming (async PBO): copy previous frame from PBO into outRGBA. Returns true if data is valid (encode/send). Call before PrepareStreamingFrame. First two frames return false (triple-buffer warmup). */
    bool GetStreamingPixelsFromPreviousFrame(uint8_t* outRGBA);
    /** Streaming (async PBO): issue readback into current PBO, then advance. Call after PrepareStreamingFrame. No-op if PBOs not available. */
    void IssueStreamingReadback();
    /** True when triple PBO path is active; use to avoid sync ReadStreamingPixels (which would trigger the sync warning). */
    bool HasStreamingPBOs() const { return m_streamingPBO[0] != nullptr; }
    /** True when GPU NV12 path is active (triple NV12 PBOs + compute pass). Prefer over RGBA when encoding. */
    bool HasStreamingNV12PBOs() const { return m_streamingNV12PBO[0] != nullptr; }
    /** Streaming (NV12): copy previous frame from NV12 PBO into outBuffer (raw, no flip). Returns true if valid. Call before PrepareStreamingFrame. outSizeBytes = pitch*height + pitch*(height/2). */
    bool GetStreamingNV12FromPreviousFrame(uint8_t* outBuffer, size_t outSizeBytes);
    /** Streaming NV12 pitch (256-byte aligned). Use with GetStreamingNV12FromPreviousFrame / SendFrameNV12. Public for GameRuntime encoder. */
    static constexpr uint32_t kStreamingPitch = 1024u;
    /** Streaming NV12 buffer size in bytes (pitch*height + pitch*(height/2)). Public for GameRuntime encoder. */
    static constexpr size_t kStreamingNV12Size = static_cast<size_t>(kStreamingPitch) * 540u + static_cast<size_t>(kStreamingPitch) * 270u;

    /** Optional XR provider. When set and session active, Render() uses stereo XR path when scene has XR camera. */
    void SetXRProvider(::eyelib::vr::IXRProvider* provider) { m_xrProvider = provider; }

    /** Device for asset loading / SetRenderDevice. Null if not initialized. */
    eyelib::api::RenderDevice GetDevice() const;

    /** M4.7 — non-null when the new pipeline is active (`kUseNewRenderPipeline == true`).
     *  Used by `RuntimeOverlay::Initialize(window, pipeline)` to bring up ImGui-Vulkan on
     *  the new device without crossing the legacy RHI seam. Null on the legacy path. */
    eyelib::api::rendering::RenderPipeline* GetRenderPipelineNew();

    /**
     * Standalone (owns swapchain): RHI BeginFrame before host records Vulkan UI and before RenderSceneOnly / BlitToScreen.
     * No-op when embedded with shared device (editor owns frame) or not ready. OpenGL IDevice: empty implementation.
     */
    void BeginPresentFrame();

    /** Create default and skinned materials once after Initialize(). Call before BindAssets/scene build so primitives are visible. No-op if already created or headless. */
    void InitializeDefaultResources();

    /** Reload pipeline shaders (e.g. after ShaderWatcher copy). */
    void ReloadShaders();
    /** Apply scene-level render mode string (Forward | Deferred | ForwardPlus). */
    void ApplySceneRenderPipelineMode(const std::string& mode);

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

    /** Standalone player only: swapchain blit + EndFrame/Present. Debug assert if called when embedded (shared device). */
    void BlitToScreenIfOwner();

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
    ModelEntry LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path);
    /** Resolve builtin:// primitive mesh id to procedural mesh + default material. */
    ModelEntry GetPrimitiveEntry(const std::string& meshId);
    std::shared_ptr<eyelib::api::TextureCube> GetOrLoadSkybox(const std::string& assetId,
                                                             const std::vector<std::string>& assetRoots);

    void BuildSceneFromViews(
        std::span<const eyelib::api::world::World::RenderableEntry> renderables,
        std::span<const eyelib::api::world::World::MeshRendererEntry> meshRenderers,
        std::span<const eyelib::api::world::World::CameraEntry> cameras,
        std::span<const eyelib::api::world::World::XRCameraEntry> xrCameras,
        std::span<const eyelib::api::world::World::LightEntry> lights,
        std::function<std::optional<::eyelib::world::components::Transform>(::eyelib::world::EntityId)> getTransform,
        const RuntimeOptions& options,
        const WorldAnimationBridge* animationBridge,
        eyelib::api::AssetRegistry* assetRegistry,
        eyelib::api::RenderScene& scene,
        api::FrameAllocator* frameAlloc,
        eyelib::api::rendering::RenderScene* parallelSceneOut = nullptr);

    void BuildParallelRenderSceneFromViews(
        std::span<const eyelib::api::world::World::RenderableEntry> renderables,
        std::span<const eyelib::api::world::World::MeshRendererEntry> meshRenderers,
        std::span<const eyelib::api::world::World::CameraEntry> cameras,
        std::span<const eyelib::api::world::World::XRCameraEntry> xrCameras,
        std::span<const eyelib::api::world::World::LightEntry> lights,
        std::function<std::optional<::eyelib::world::components::Transform>(::eyelib::world::EntityId)> getTransform,
        const RuntimeOptions& options,
        const WorldAnimationBridge* animationBridge,
        eyelib::api::AssetRegistry* assetRegistry,
        eyelib::api::rendering::RenderScene& outNew);

    eyelib::api::Renderer m_renderer;
    eyelib::api::RenderPipeline m_pipeline;
    /** M4.1 — parallel `api/rendering` stack (cold until M4.4 flip). Not Initialized as a
     *  second VkDevice/swapchain on the same `Window` — that would fight the legacy stack.
     *  `ResourceCache` is constructed with a null device so the type links + stays inert. */
    eyelib::api::rendering::Renderer       m_rendererNew;
    eyelib::api::rendering::RenderPipeline m_pipelineNew;
    std::unique_ptr<eyelib::api::rendering::ResourceCache> m_resourceCacheNew;
    /** M4.3 — parallel resource resolver + builtin manager built atop `m_resourceCacheNew`.
     *  Inert while the cache is cold; the M4.4 flip rebuilds the cache against a real
     *  device and these adapters become live in lock-step. */
    std::unique_ptr<RendererGPUResourceCacheNew> m_gpuResourceCacheNew;
    std::unique_ptr<BuiltinResourceManagerNew>   m_builtinManagerNew;
    /** Owned skybox cache used when no external skybox cache is provided. */
    std::unique_ptr<SkyboxCacheNew> m_skyboxCacheNewOwned;
    /** Borrowed editor-owned caches (non-owning). */
    eyelib::api::rendering::ResourceCache* m_externalResourceCacheNew = nullptr;
    RendererGPUResourceCacheNew*           m_externalGpuResourceCacheNew = nullptr;
    BuiltinResourceManagerNew*             m_externalBuiltinManagerNew = nullptr;
    SkyboxCacheNew*                        m_externalSkyboxCacheNew = nullptr;
    /** M4.4 — runtime flag mirroring `kUseNewRenderPipeline` for the active `Initialize`
     *  call (standalone only; embedded mode forces it false). All hot-path methods branch
     *  on this so the legacy stack stays the source of truth when off. */
    bool m_usingNewPipeline = false;
    bool m_ready = false;
    bool m_ownsPresentation = true;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::unique_ptr<BuiltinResourceManager> m_builtinManager;
    eyelib::api::AssetRegistry* m_assetRegistryForResolver = nullptr;
    std::unique_ptr<RendererGPUResourceCache> m_gpuResourceCache;
    std::shared_ptr<eyelib::api::Shader> m_defaultShader;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::Shader> m_skinnedShader;
    std::vector<SkinnedMaterialEntry> m_skinnedMaterials;
    std::unordered_map<std::string, std::shared_ptr<eyelib::api::TextureCube>> m_skyboxCache;
    eyelib::api::world::World::RenderSnapshot m_frameSnapshot;

    ::eyelib::vr::IXRProvider* m_xrProvider = nullptr;
    std::unique_ptr<eyelib::api::RenderTarget> m_xrEyeTargets[2];
    uint32_t m_xrEyeSize[2][2] = {{0, 0}, {0, 0}};

    static constexpr uint32_t kStreamingWidth = 960u;
    static constexpr uint32_t kStreamingHeight = 540u;
    static constexpr int kStreamingPBOCount = 3;
    static constexpr int kStreamingRTCount = 3;
    std::unique_ptr<eyelib::api::RenderTarget> m_streamingTargets[kStreamingRTCount];
    int m_streamingRTWriteIndex = 0;
    int m_streamingRTFrameCount = 0;
    void* m_streamingPBO[kStreamingPBOCount] = {nullptr, nullptr, nullptr};
    int m_streamingPBOIndex = 0;
    int m_streamingReadbackCount = 0;
    void* m_streamingNV12PBO[kStreamingPBOCount] = {nullptr, nullptr, nullptr};
    int m_streamingNV12PBOIndex = 0;
    int m_streamingNV12ReadbackCount = 0;
};

} // namespace openeye::player
