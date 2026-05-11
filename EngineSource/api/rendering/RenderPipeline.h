#pragma once

#include "api/rendering/CameraView.h"
#include "api/rendering/RenderBackend.h"
#include "api/rendering/RenderDevice.h"

#include "eyelib/math/Types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace eyelib {
namespace api {
class Window;
}}

namespace eyelib {
namespace api {
namespace rendering {

struct RenderScene;          // M1.5 — POD; see api/rendering/RenderScene.h
struct MaterialRecord;       // M3.7b — POD; see api/rendering/MaterialRecord.h
struct ImGuiVulkanInitInfo;  // M4.7 — POD; see api/rendering/ImGuiVulkanInitInfo.h
class  RenderTarget;         // M3.1 / M4.5
class  ResourceCache;        // M3.7b — see api/rendering/ResourceCache.h

/**
 * @brief Public render pipeline facade (PIMPL over `eyelib::rendering::RenderWorld`).
 *
 * Owns:
 *  - The RHI device when constructed via `Initialize(Window&, ...)` (Vulkan-only — see
 *    `RenderBackend`); a borrowed device when constructed via `InitializeWithSharedDevice`.
 *  - A `eyelib::rendering::RenderWorld` (frame graph + feature registry + scheduler).
 *  - A default `RasterFeature` (deferred / forward) and a `ShadowFeature`. Additional
 *    features can be plugged via the M1.7 hooks (debug, picking) once those land.
 *
 * Does NOT expose:
 *  - Any `eyelib::rendering::*` or `eyelib::rendering::rhi::*` type. All scene data
 *    crosses the boundary as `RenderScene` (POD; M1.5) and all device handles cross as
 *    `RenderDevice` (opaque pointer).
 *
 * Thread safety: not thread-safe. One pipeline per render context; the editor uses up to
 * three (scene view, game view, material preview) — each on its own pipeline instance.
 *
 * Lifecycle (typical owning case — editor/player main pipeline):
 *   1. Construct on the render thread.
 *   2. `Initialize(window, w, h)` — creates a Vulkan device + swapchain, registers
 *      Raster + Shadow features, calls `RenderWorld::Initialize`.
 *   3. Per frame: `UpdateUploads()` (no-op in v1; reserved), `Render(scene)`, optional
 *      `BlitToScreen()` (currently a no-op — `RasterFeature` already writes the swapchain
 *      via its blit pass), `Finish()` before swap.
 *   4. `Resize(w, h)` on framebuffer resize events.
 *   5. `Shutdown()` then destruct (or rely on dtor).
 *
 * Lifecycle (shared device — editor's secondary pipelines):
 *   1. Construct on the render thread.
 *   2. `InitializeWithSharedDevice(device, w, h)` (or the `shareFrom` overload when shader
 *      sharing is wired — see step notes for current parity gap).
 *
 * **Migration parity (legacy `eyelib::api::RenderPipeline`):**
 *   - Method names + signatures match the legacy header so editor/player call sites
 *     migrate by namespace + type substitution only.
 *   - Subset shipped here = orchestration + lifecycle + scene-view bring-up. The
 *     debug/picking surface (`Render` overload with entity IDs, `RenderDebugLines`,
 *     shadow debug accessors, profiler accessor) lands in **M1.7**. Readback / PBO /
 *     NV12 / stereo / XR / GPU fences land in **M1.8**.
 *   - `Mode::Hybrid` maps to the new module's `RenderMode::Deferred`. `ForwardPlus` is
 *     not yet implemented in the new module; setting it logs a one-time warning and
 *     falls back to `Forward` with a one-time warning.
 */
class RenderPipeline {
public:
    /** Hybrid = GBuffer + deferred lighting (default; equivalent to legacy "Hybrid").
     *  Forward = forward opaque baseline. ForwardPlus = not yet implemented (falls back
     *  to Forward with a warning). */
    enum class Mode { Hybrid, Forward, ForwardPlus };

    /** Where a pipeline writes its final color.
     *
     *  - `Direct`   — the pipeline composes its main HDR target to the swapchain inside its
     *                 own frame graph (`BlitMainHdrToDefaultPass`). This is the single-pipeline
     *                 player shape: `Render(scene)` produces a presentable swapchain image.
     *  - `Offscreen` — the pipeline produces an HDR offscreen target only; the caller is the
     *                 swapchain compositor and drives swapchain composition explicitly (see
     *                 `ComposeSwapchain()`). This is the editor multi-pipeline shape, where
     *                 scene-view / game-view / material-preview each render into their own
     *                 HDR texture, sampled by ImGui as viewport contents, and a single editor
     *                 compose step at end-of-frame writes the swapchain.
     *
     *  Defaults:
     *   - `Initialize(window, ...)`         → `Direct`   (single-pipeline player default).
     *   - `InitializeWithSharedDevice(...)` → `Offscreen` (multi-pipeline producer default).
     *
     *  The editor flips its window-owning pipeline to `Offscreen` explicitly because device
     *  ownership and compositor role are orthogonal: the editor pipeline owns the device but
     *  is **not** the compositor (the editor app is). Callers may call `SetPresentationMode`
     *  any time after `Initialize` / `InitializeWithSharedDevice`; the change takes effect on
     *  the next `Render`. */
    enum class PresentationMode { Direct, Offscreen };

    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&& other) noexcept;
    RenderPipeline& operator=(RenderPipeline&& other) noexcept;

    /** Create device and pipeline. Vulkan-only — `backend` other than `Vulkan` is normalized
     *  with a warning. Owning lifetime: this pipeline destroys the device in `Shutdown`. */
    bool Initialize(Window& window, uint32_t width, uint32_t height,
                    RenderBackend backend = RenderBackend::Default);
    /** Initialize with a shared RHI device (caller owns device). Borrowed lifetime. */
    bool InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height);
    /** Initialize with shared device and reuse shader/pipeline cache from another pipeline.
     *  In v1 this falls back to a fresh shader cache and logs a one-time warning — the
     *  new module's per-feature shader ownership does not yet expose a shared cache. */
    bool InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height,
                                    RenderPipeline* shareFrom);

    /** Resize the pipeline's *render output* (offscreen HDR + downstream RTs) to `width x height`.
     *  Does NOT touch the window swapchain — that has its own contract via `OnWindowResized`.
     *  Safe to call every frame with viewport-panel dimensions; the implementation only stores
     *  the new size and the features' RTs reallocate lazily on mismatch at the next `Render`. */
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Set whether this pipeline composes to the swapchain (`Direct`) or only produces an
     *  offscreen HDR target for caller-driven compose (`Offscreen`). Safe to call any time
     *  after `Initialize` / `InitializeWithSharedDevice`; the change takes effect on the
     *  next `Render`. See `PresentationMode` for the contract + defaults. */
    void SetPresentationMode(PresentationMode mode);
    PresentationMode GetPresentationMode() const;

    /** Cubemap sky HDR multiplier (default 1; clamped non-negative). Stored as parity slot;
     *  plumbing into the new module's IBL probes lands when the editor sky panel migrates
     *  (M3) or when the new module exposes a sky-brightness control (whichever first). */
    void SetSkyboxBrightness(float multiplier);
    float GetSkyboxBrightness() const;

    /** Reload all pipeline shaders. v1 stub: logs a warning that hot-reload is not yet wired
     *  through the new module's `ShaderLibrary`; the request is silently dropped. */
    void ReloadShaders();

    /** Upload phase (CPU→staging→vkCmdCopy*). v1 no-op: the new module's `RunFrame` performs
     *  upload + execute together; the call stays on the API surface so editor/player code
     *  with explicit upload/render split continues to compile. */
    void UpdateUploads();

    // ===== M3.9 — Device-level frame boundary (multi-pipeline editor support) =========
    //
    // The editor renders 3 pipelines per frame (scene / game / material preview) all
    // offscreen, then composits with ImGui onto one swapchain image. The standalone player
    // renders 1 pipeline per frame and presents. Both flows drive the device frame boundary
    // explicitly through the four methods below; `Render(scene)` is record-only (does not
    // call BeginFrame/EndFrame) — matching the legacy editor's contract.
    //
    //   Editor frame:  BeginPresentFrame() → Render(sceneA/B/C) → ImGui record →
    //                  EndPresentFrame() → Present()
    //   Player frame:  BeginPresentFrame() → Render(scene) → BlitToScreen() →
    //                  EndPresentFrame() → Present()
    //
    // When 3 pipelines share a device (editor), any one of them may drive
    // `BeginPresentFrame`/`EndPresentFrame`/`Present` since these forward to `IDevice` which
    // is shared. By convention drive them on the primary/scene pipeline.

    /** Open a device frame (`IDevice::BeginFrame`). `Render(scene)` calls between this and
     *  `EndPresentFrame()` record into the active command list. No-op when pipeline not
     *  initialized. */
    void BeginPresentFrame();

    /** Close the device frame (`IDevice::EndFrame`). Pair with a prior `BeginPresentFrame()`.
     *  Call `Present()` after to actually submit + present the swapchain image. No-op when
     *  pipeline not initialized. */
    void EndPresentFrame();

    /** Present the swapchain image (`IDevice::Present`). Independent of frame open/closed
     *  state — caller is responsible for ordering. No-op when pipeline not initialized. */
    void Present();

    /** Notify the device of a window framebuffer resize (`IDevice::OnWindowResized`). Does
     *  NOT call `Resize` on the pipeline's offscreen RTs — call that separately if needed.
     *  No-op when pipeline not initialized. */
    void OnWindowResized(uint32_t width, uint32_t height);

    /** Compose the swapchain image: clear → swapchain render pass → invoke the post-blit
     *  callback (where ImGui's draw data is recorded). Used by the editor multi-pipeline flow
     *  (3 pipelines render offscreen via `Render`, then a single `ComposeSwapchain()` runs
     *  the swapchain pass once). The standalone player flow already gets this inside
     *  `Render(scene)` via `RasterFeature::BlitMainHdrToDefaultPass`, so it must not call
     *  `ComposeSwapchain` (would double-fire the post-blit callback). Calls
     *  `IDevice::BlitToDefaultFramebuffer(nullptr)` internally; no-op when not initialized. */
    void ComposeSwapchain();

    /** Record the scene into the active command list (assumes `BeginPresentFrame()` has been
     *  called). Does NOT call `BeginFrame`/`EndFrame` itself. Returns immediately if the
     *  pipeline / device / world is not initialized. */
    void Render(const RenderScene& scene);

    /** Blit pipeline output to the swapchain. v1 no-op: `RasterFeature` already includes
     *  `BlitMainHdrToDefaultPass` in the frame graph, so the blit happens inside `Render`.
     *  The call stays on the API surface for legacy parity. */
    void BlitToScreen();

    /** Restore default framebuffer + viewport for subsequent ImGui drawing. v1 no-op: the
     *  new module's frame graph executor leaves the swapchain bound after the final blit. */
    void RestoreDefaultFramebufferAndViewport(uint32_t width, uint32_t height);

    /** Wait for GPU to finish all submitted work. Calls `IDevice::Finish`. */
    void Finish();

    /** True when initialized + has produced output at least once (pipeline + device + world). */
    bool HasValidOutput() const;

    /** True when this pipeline owns its device (created via `Initialize(Window&, ...)`). */
    bool OwnsDevice() const;

    /** Device handle (opaque). Null when not initialized. Use to pass into asset-side GPU
     *  resource creation, ImGui Vulkan init, etc. */
    RenderDevice GetDevice() const;

    /** Active backend name ("vulkan"). Always valid post-Initialize. */
    const char* GetBackendName() const;

    /** Typed backend identity. */
    RenderBackend GetBackend() const;

    /** Native texture handle of the pipeline output (post-blit color). For ImGui Vulkan:
     *  cast to `VkImageView` (wrapped via `ImGui_ImplVulkan_AddTexture`). Null until
     *  `Render` has produced at least one frame. */
    void* GetOutputColorTextureNativeHandle() const;

    // ===== M4.7 — ImGui-Vulkan integration =========================================
    //
    // Both the editor (`ScopedImGui`) and the standalone player (`RuntimeOverlay`) need to
    // bring up Dear ImGui on top of the new pipeline's VkDevice. The two methods below
    // expose the minimum surface for that without leaking `eyelib::rendering::rhi::*`
    // types past the façade boundary:
    //   - `GetImGuiVulkanInitInfo(out)` populates a public POD that maps 1:1 to
    //     `ImGui_ImplVulkan_InitInfo` (caller casts the `void*`s back to Vulkan handles
    //     at the ImGui glue site — same pattern as the legacy stack).
    //   - `SetPostBlitCallback(cb)` registers a callback the new module's RHI invokes
    //     after the swapchain blit pass finishes recording, so ImGui can append its own
    //     draw commands into the same command buffer (`ImGui_ImplVulkan_RenderDrawData`).
    // Both methods delegate to the underlying `IDevice`. They are no-ops + return false /
    // ignore when the pipeline is not initialized or the backend is not Vulkan.

    /** Fill `out` with VkInstance / VkPhysicalDevice / VkDevice / queue / descriptor-pool /
     *  render-pass for ImGui_ImplVulkan_Init. Returns false if not initialized. */
    bool GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) const;

    /** Register `cb(VkCommandBuffer*)` to run after the swapchain blit. ImGui draw recording
     *  goes here. Pass `{}` (default-constructed `std::function`) to clear. */
    void SetPostBlitCallback(std::function<void(void*)> cb);

    /** M3.9 — opaque default `VkSampler` from the underlying Vulkan device (cast to `void*`
     *  at the boundary; callers cast back to `VkSampler`). Used by editor `ScopedImGui` to
     *  build viewport descriptor sets (combined-image-sampler) without `static_cast`ing the
     *  device to the legacy `eyelib::render::rhi::DeviceVK*`. Returns `nullptr` when the
     *  pipeline is not initialized, the backend is not Vulkan, or the underlying device
     *  did not create a default sampler. The sampler is owned by the device; callers MUST
     *  NOT `vkDestroySampler` it. Lifetime spans `Initialize` → `Shutdown`. */
    void* GetDefaultVulkanSampler() const;

    // ===== M3.7b — Resource cache integration ========================================
    //
    // The pipeline routes mesh handle lookups (per-draw vertex/index buffer fetch) and
    // material-record reads (per-instance PBR row in the GBuffer pass) through the
    // attached `ResourceCache`. Editor / player call `AttachResourceCache(cache)` once
    // after both the pipeline and the cache exist; subsequent `cache->Create*` calls
    // automatically push their data into this pipeline (mesh resolver entry + material
    // table row).
    //
    // Attaching N pipelines to one cache is supported (editor scene + game + material
    // preview all share assets). Detach before destroying either side.

    /** Bind `cache` as this pipeline's mesh resolver and prime its `RenderResources::MaterialTable`
     *  from the cache's current records. Idempotent for the same cache instance; calling with
     *  a different cache implicitly detaches the previous one. */
    void AttachResourceCache(ResourceCache& cache);

    /** Reverse of `AttachResourceCache`. Safe to call multiple times. */
    void DetachResourceCache(ResourceCache& cache);

    /** Push (or update) a single material-table row. Called by `ResourceCache` whenever
     *  a record is created / mutated; editor / player do not call this directly. The
     *  pipeline owns the eventual GPU upload through `RenderResources::UploadMaterialTableFromDrawList`. */
    void SetMaterialRecord(uint32_t materialId, const MaterialRecord& record);

    // ===== M1.7 — Debug hooks parity =================================================
    //
    // Two categories:
    //  (A) **Active** accessors — shadow debug + per-pass profiler + output texture.
    //      Editor panels (`ShadowDebugPanel`, `ProfilerPanel`) consume these.
    //  (B) **Stub** — GPU picking + immediate / pending debug-line drawing.
    //      Call sites compile after migration; each path is a no-op with a one-time warning.
    //      EntityIDPass / DebugDrawPass features exist in the render module but are not wired here yet.

    /** Debug: Vulkan `VkImageView` (void*) of the directional CSM depth (2D-array image),
     *  obtained from the frame-graph blackboard slot `DirectionalShadowMapArray`. Null when
     *  shadows have not produced output yet (e.g. pre-first-Render or shadow disabled). */
    void* GetShadowDepthMapNativeViewForDebug() const;
    /** Debug: cascade split count for CSM. Returns the new module's fixed CSM cascade count
     *  (`ShadowFeature::m_lightViewProj` size = 3). Returns 0 if shadows are off. */
    uint32_t GetShadowCascadeCountForDebug() const;
    /** Debug: shadow depth-map per-cascade resolution (single-cascade width = height). 0
     *  if no shadow render target has been published. */
    uint32_t GetShadowMapResolutionForDebug() const;

    /** Profiler: per-pass GPU/CPU times from the frame graph. Updated each `Render`. */
    struct FrameGraphPassInfo {
        std::string name;
        double      gpuMs  = 0.0;
        bool        culled = false;
    };
    /** v1: returns an empty vector. The new module's `GpuProfiler` exposes pipeline-statistics
     *  totals only (vertex/primitive/clip counters), not per-pass timestamps yet.
     *  The editor `ProfilerPanel` can fall back to CPU-side ImGui pass timing until timestamps land. */
    std::vector<FrameGraphPassInfo> GetFrameGraphPasses() const;

    // ----- (B) Documented pause: GPU picking ----------------------------------------
    /** Render scene + (paused in v1) GPU picking. The `entityIdsForOpaque/Transparent`
     *  arguments are silently ignored; this overload calls `Render(scene)`. */
    void Render(const RenderScene& scene,
                const std::vector<uint32_t>* entityIdsForOpaque,
                const std::vector<uint32_t>* entityIdsForTransparent = nullptr);
    /** Paused: returns false. */
    bool ReadPixelEntityId(uint32_t x, uint32_t y, uint32_t* outId) const;
    /** Paused: no-op. */
    void RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r = 1.0f, float g = 0.5f, float b = 0.0f);

    // ----- (B) Documented pause: debug lines ----------------------------------------
    /** Paused: no-op. */
    void RenderDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
                          const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines,
                          float r, float g, float b);
    /** Paused: no-op. */
    void SetPendingDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
                              const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines,
                              float r, float g, float b);
    /** Paused: no-op. */
    void SetPendingDebugLinesColored(
        const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
        const std::vector<std::pair<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>, eyelib::math::Vec4>>& linesWithColors);

    // ===== M1.8 — Output / readback / streaming / stereo / XR / fence parity =========
    //
    // The legacy `eyelib::api::RenderPipeline` exposes a wide surface for player streaming
    // (sync RGBA readback, async PBO ring, NV12 compute pass), XR (single-pass stereo
    // render + per-eye blit), and explicit GPU fence handshakes. **None of these have a v1
    // client** in the migration plan: the editor scene-view (M2 / M3) does not need any
    // of them; the player main render path (M4) needs a subset; XR is fully paused on this API for now.
    //
    // All methods below keep legacy signatures so call sites compile. Each stub returns a safe
    // deterministic value; unimplemented paths log once per pipeline instance via Log::Warn.

    /** Resize using an offscreen target's dimensions (e.g. XR eye buffer). Paused. */
    void Resize(const RenderTarget& target);

    // ----- Sync swapchain / RT readback (M4 player streaming) -----------------------
    /** Read current frame output as RGBA8. Paused: returns false; `outRGBA` untouched. */
    bool ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA) const;
    /** Read pixels from an arbitrary render target. Paused: returns false. */
    bool ReadPixelsFromRenderTarget(const RenderTarget& target, uint8_t* outRGBA) const;

    // ----- Async PBO readback ring (M4 player streaming) ----------------------------
    /** Allocate an async pixel-pack buffer (PBO) for streaming readbacks. Paused: returns null. */
    void* CreatePixelPackBuffer(size_t sizeInBytes) const;
    /** Free an async pixel-pack buffer. Paused: no-op (also safe on null). */
    void DestroyPixelPackBuffer(void* handle) const;
    /** Issue async readback of a render target into a PBO. Paused: no-op. */
    void ReadPixelsToPBO(const RenderTarget& target, void* pboHandle) const;
    /** Map PBO + copy with optional Y-flip into `outBuffer`. Paused: zero-fills `outBuffer`. */
    void MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes,
                            uint32_t width, uint32_t height) const;
    /** Map PBO + copy without Y-flip (for NV12 readback). Paused: zero-fills. */
    void MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) const;

    // ----- NV12 streaming compute pass (M4 player streaming) ------------------------
    /** Compute pass: source RT (RGBA) -> NV12 into the given PBO. Paused: no-op. */
    void RunStreamingNV12Pass(const RenderTarget& sourceRT, void* nv12PBOHandle,
                              size_t nv12Size, uint32_t width, uint32_t height, uint32_t pitch) const;

    // ----- XR / stereo (M4 player XR; documented pause) -----------------------------
    /** Single-pass stereo render (left | right viewport). Paused: delegates to `Render(scene)`. */
    void RenderStereo(const RenderScene& scene, const CameraView& leftCam, const CameraView& rightCam);
    /** Blit current pipeline output to an offscreen target (XR eye). Paused: no-op. */
    void BlitOutputTo(RenderTarget& target);
    /** Blit left + right RTs side-by-side to swapchain (stereo preview). Paused: no-op. */
    void BlitStereoToScreen(RenderTarget& left, RenderTarget& right,
                            uint32_t totalWidth, uint32_t totalHeight);

    // ----- Explicit GPU fences (M4 player swap handshake) ---------------------------
    /** Insert a GPU fence after current command stream. Paused: returns null. */
    void* InsertFrameFence();
    /** Block on `fence` (kept alive). Paused: no-op. */
    void WaitFence(void* fence);
    /** Block on `fence` then destroy it. Paused: no-op. */
    void WaitAndDestroyFence(void* fence);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
