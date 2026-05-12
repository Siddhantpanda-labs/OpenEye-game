#include "api/rendering/RenderPipeline.h"

#include "api/core/Log.h"
#include "api/platform/Window.h"
#include "api/rendering/ImGuiVulkanInitInfo.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/RenderScene.h"
#include "api/rendering/ResourceCache.h"
#include "api/rendering/Skybox.h"
#include "api/rendering/internal/ResourceAccess.h"

#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/features/editor/DebugLine.h"
#include "eyelib/rendering/features/editor/EditorFeature.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/features/raster/SkyboxFeature.h"
#include "eyelib/rendering/features/raster/SkyboxRequest.h"
#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/lighting/SceneLighting.h"
#include "eyelib/rendering/materials/MaterialSchema.h"
#include "eyelib/rendering/materials/MaterialTable.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/Vulkan/core/CreateDeviceVulkan.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace eyelib {
namespace api {
namespace rendering {

namespace {
// Map the public Mode enum to the new module's RasterFeature::RenderMode.
// `Hybrid` -> `Deferred` (default), `Forward` -> `Forward`, `ForwardPlus` -> `Forward`
// with a one-time warning (ForwardPlus not implemented on this path yet).
eyelib::rendering::RenderMode ToInternalMode(RenderPipeline::Mode mode, bool& outWarnForwardPlusFallback) {
    outWarnForwardPlusFallback = false;
    switch (mode) {
        case RenderPipeline::Mode::Hybrid:      return eyelib::rendering::RenderMode::Deferred;
        case RenderPipeline::Mode::Forward:     return eyelib::rendering::RenderMode::Forward;
        case RenderPipeline::Mode::ForwardPlus:
            outWarnForwardPlusFallback = true;
            return eyelib::rendering::RenderMode::Forward;
    }
    return eyelib::rendering::RenderMode::Deferred;
}

// Map the public alpha-mode enum to the internal one (1:1 numeric values asserted in
// MaterialRecord.h). Cast guarded to the documented [0..2] range.
eyelib::rendering::materials::MaterialAlphaMode ToInternalAlphaMode(MaterialAlphaMode m) {
    const uint32_t v = static_cast<uint32_t>(m);
    return static_cast<eyelib::rendering::materials::MaterialAlphaMode>(v <= 2u ? v : 0u);
}

// Translate a public POD `MaterialRecord` into the internal `materials::MaterialRecord`
// that lives on the new module's MaterialTable. One-to-one field copy; no validation
// (`MaterialTable::SetMaterial` already clamps via `FillMaterialGpuRow`).
eyelib::rendering::materials::MaterialRecord ToInternalRecord(const MaterialRecord& src) {
    eyelib::rendering::materials::MaterialRecord out{};
    out.baseColorFactor       = src.baseColorFactor;
    out.emissiveFactor        = src.emissiveFactor;
    out.metallicFactor        = src.metallicFactor;
    out.roughnessFactor       = src.roughnessFactor;
    out.normalScale           = src.normalScale;
    out.occlusionStrength     = src.occlusionStrength;
    out.alphaCutoff           = src.alphaCutoff;
    out.alphaMode             = ToInternalAlphaMode(src.alphaMode);
    out.doubleSided           = src.doubleSided;
    out.baseColorTexture      = src.baseColorTexture;
    out.normalTexture         = src.normalTexture;
    out.metallicRoughnessTexture = src.metallicRoughnessTexture;
    out.emissiveTexture       = src.emissiveTexture;
    out.occlusionTexture      = src.occlusionTexture;
    return out;
}

// =====================================================================================
// M4.0a — Per-frame light publication into eyelib::rendering::SceneLighting.
//
// The new module's lighting bus is a process-global (`SceneLighting` static API). The
// adapter clears point/spot arrays and sets the directional slot from the public
// `RenderScene` each `Render` call, mirroring the legacy player BuildScene behaviour:
//   ClearPointLights + ClearSpotLights are unconditional (so the adapter is the source
//   of truth for the upcoming frame); SetDirectionalLight runs only when the public
//   `directionalLightActive` flag is set (preserving the legacy "missing dir = keep
//   previous" semantics — see RenderScene.h docstring).
//
// SceneLighting silently caps point lights at `kMaxScenePointLights = 8` and spot lights
// at 16 (internal hard cap in `SceneLighting::AddSpotLight`). Public consumers should
// pre-trim if exact counts matter.
//
// Why here (not inside `RenderSceneAdapter::Reset`):
//   - The adapter is logically "view-bound" data (`IRenderSceneView` contract). Lights
//     are out-of-band global state on the new module — keeping the push next to
//     `Render` makes the lifecycle match where `SceneLighting` actually gets read.
// =====================================================================================
void PublishLightsToSceneLighting(const RenderScene& scene) {
    // Directional: legacy semantics — only update when the scene reports an active sun;
    // missing directional intentionally preserves the previous frame's value (some games
    // set the sun once at scene load and never touch it again).
    if (scene.directionalLightActive) {
        const auto& dl = scene.directionalLight;
        // SceneLighting::SetDirectionalLight does its own normalize-if-non-zero internally.
        eyelib::rendering::SceneLighting::SetDirectionalLight(dl.direction, dl.color, dl.intensity);
        // Note: `castShadows` is honoured by `ShadowFeature` per-frame via a separate
        // hook in the new module; once that hook lands, plumb here. Today it is on by
        // default in the feature so a `false` request silently has no effect — a follow
        // up for `Render_steps_todo.md` Section C.
    }

    eyelib::rendering::SceneLighting::ClearPointLights();
    for (const auto& p : scene.pointLights) {
        eyelib::rendering::SceneLighting::PointLightSceneState s{};
        s.position    = p.position;
        s.range       = p.range;
        s.color       = p.color;
        s.intensity   = p.intensity;
        s.castShadows = p.castShadows;
        eyelib::rendering::SceneLighting::AddPointLight(s);
        // SceneLighting silently drops past `kMaxScenePointLights`. Loop continues so
        // the count reported via `GetPointLights` matches what the bus actually has.
    }

    eyelib::rendering::SceneLighting::ClearSpotLights();
    for (const auto& p : scene.spotLights) {
        eyelib::rendering::SceneLighting::SpotLightSceneState s{};
        s.position     = p.position;
        s.range        = p.range;
        s.direction    = p.direction;
        s.intensity    = p.intensity;
        s.color        = p.color;
        s.innerConeCos = p.innerConeCos;
        s.outerConeCos = p.outerConeCos;
        s.castShadows  = p.castShadows;
        eyelib::rendering::SceneLighting::AddSpotLight(s);
    }
}

// Map the public DrawLayer to the new module's DrawLayer (1:1 by design).
eyelib::rendering::DrawLayer ToInternalLayer(DrawLayer layer) {
    switch (layer) {
        case DrawLayer::Opaque:      return eyelib::rendering::DrawLayer::Opaque;
        case DrawLayer::AlphaTest:   return eyelib::rendering::DrawLayer::AlphaTest;
        case DrawLayer::Transparent: return eyelib::rendering::DrawLayer::Transparent;
        case DrawLayer::Sky:         return eyelib::rendering::DrawLayer::Sky;
    }
    return eyelib::rendering::DrawLayer::Opaque;
}

/**
 * Internal façade adapter: turns the public POD `RenderScene` into the read-only contract
 * (`eyelib::rendering::IRenderSceneView`) consumed by `RenderWorld::RunFrame`. Owns the
 * camera + flat instance buffer; refilled per `RenderPipeline::Render` call.
 *
 * Lives entirely inside this TU — never exposed across the api/rendering boundary.
 * Storage is reused across frames (vector::clear keeps capacity).
 */
class RenderSceneAdapter final : public eyelib::rendering::IRenderSceneView {
public:
    void Reset(const RenderScene& scene) {
        m_camera.view       = scene.camera.view;
        m_camera.projection = scene.camera.projection;
        m_camera.viewPos    = scene.camera.viewPos;
        m_camera.nearPlane  = scene.camera.nearPlane;
        m_camera.farPlane   = scene.camera.farPlane;

        m_instances.clear();
        m_instances.reserve(scene.opaque.size() + scene.transparent.size());

        uint32_t transformIndex = 0;
        for (const auto& d : scene.opaque) {
            eyelib::rendering::SceneDrawInstance s{};
            s.meshHandle     = d.meshHandle;
            s.materialId     = d.materialId;
            s.transformIndex = transformIndex++;
            s.worldTransform = d.worldTransform;
            s.localBounds    = d.localBounds;
            // Public DrawableInstance.layer may be Opaque / AlphaTest / Sky; the public
            // RenderScene buckets transparent into its own array, so for opaque-bucket
            // instances we honour the per-instance layer (allows Sky / AlphaTest).
            s.layer = ToInternalLayer(d.layer == DrawLayer::Transparent ? DrawLayer::Opaque : d.layer);
            s.flags = d.flags;
            m_instances.push_back(s);
        }
        for (const auto& d : scene.transparent) {
            eyelib::rendering::SceneDrawInstance s{};
            s.meshHandle     = d.meshHandle;
            s.materialId     = d.materialId;
            s.transformIndex = transformIndex++;
            s.worldTransform = d.worldTransform;
            s.localBounds    = d.localBounds;
            // Force Transparent for instances in the transparent bucket regardless of the
            // per-instance layer field (the bucket is the source of truth).
            s.layer = eyelib::rendering::DrawLayer::Transparent;
            s.flags = d.flags;
            m_instances.push_back(s);
        }
    }

    const eyelib::rendering::CameraView* TryGetCamera() const override { return &m_camera; }
    size_t SceneDrawInstanceCount() const override { return m_instances.size(); }
    const eyelib::rendering::SceneDrawInstance* SceneDrawInstances() const override {
        return m_instances.empty() ? nullptr : m_instances.data();
    }

private:
    eyelib::rendering::CameraView m_camera{};
    std::vector<eyelib::rendering::SceneDrawInstance> m_instances;
};

}  // namespace

class RenderPipeline::Impl {
public:
    // Device: owned when created via Initialize(Window&, ...); borrowed via InitializeWithSharedDevice.
    std::unique_ptr<eyelib::rendering::rhi::IDevice> ownedDevice;
    eyelib::rendering::rhi::IDevice* device = nullptr;
    bool ownsDevice = false;

    // Frame orchestration.
    std::unique_ptr<eyelib::rendering::RenderWorld> world;
    std::unique_ptr<eyelib::rendering::RasterFeature> raster;
    std::unique_ptr<eyelib::rendering::ShadowFeature> shadow;
    // Optional sky feature (cubemap + procedural). Always subscribed so the per-frame
    // `RenderScene::skybox` push lights it up without a re-subscribe handshake; truly
    // zero per-frame cost when no caller supplies a skybox (`OnSchedulePasses` no-ops
    // when the request kind is `None`).
    std::unique_ptr<eyelib::rendering::SkyboxFeature> sky;
    // Editor overlay feature (debug-line drawing today; entity-ID pass future). Always subscribed
    // so the SetPendingDebugLines* / RenderDebugLines surfaces work without a re-subscribe handshake;
    // truly zero per-frame cost when no caller has supplied lines (`OnSchedulePasses` skips the pass).
    std::unique_ptr<eyelib::rendering::EditorFeature> editor;

    // Resource cache attachment (M3.7b). The cache plays two roles for the pipeline:
    //   1. Mesh resolver: the cache supplies the `IMeshHandleResolver*` consumed by the
    //      raster + shadow features (they need it before any draw runs).
    //   2. Material publisher: the cache pushes `MaterialRecord` rows into
    //      `RenderResources::MaterialTable` via `RenderPipeline::SetMaterialRecord`.
    // Lifetime: borrowed; the editor / player owns the cache and detaches before either
    // side is destroyed.
    ResourceCache* attachedCache = nullptr;

    // Adapter from public POD `RenderScene` to internal `IRenderSceneView`. Owned here so
    // its instance buffer is reused across frames (vector capacity preserved).
    RenderSceneAdapter sceneAdapter{};

    // Public-surface state (kept here so move semantics on RenderPipeline are trivial).
    Mode mode = Mode::Hybrid;
    // Default Direct = single-pipeline player shape (compose to swapchain inside the frame
    // graph). The shared-device init flips this to Offscreen before BuildWorldAndFeatures
    // runs (editor multi-pipeline shape; editor app is the compositor).
    PresentationMode presentationMode = PresentationMode::Direct;
    float skyboxBrightness = 1.0f;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    bool hasRenderedAtLeastOnce = false;

    // One-time-warning latches.
    bool warnedForwardPlusFallback = false;
    bool warnedReloadShadersUnimplemented = false;
    bool warnedSharedShareFromUnimplemented = false;
    bool warnedNonVulkanBackend = false;
    // M1.7 paused-feature latches (one warn per pipeline instance per feature).
    bool warnedGpuPickingPaused = false;
    // M1.8 paused-feature latches.
    bool warnedReadbackPaused = false;
    bool warnedPboPaused = false;
    bool warnedNv12Paused = false;
    bool warnedStereoPaused = false;
    bool warnedFencePaused = false;
    bool warnedRtResizePaused = false;

    bool BuildWorldAndFeatures(uint32_t width, uint32_t height) {
        if (!device) return false;
        outputWidth = width;
        outputHeight = height;

        world = std::make_unique<eyelib::rendering::RenderWorld>(device);
        world->SetOutputSize(width, height);

        raster = std::make_unique<eyelib::rendering::RasterFeature>();
        bool warnFwd = false;
        raster->SetRenderMode(ToInternalMode(mode, warnFwd));
        // Explicit contract (see RenderPipeline::PresentationMode): Direct schedules the
        // in-graph swapchain blit (single-pipeline player); Offscreen omits it (caller is
        // the compositor — multi-pipeline editor). Device ownership and compositor role
        // are orthogonal; this replaces an earlier ownsDevice heuristic that wrongly
        // forced the editor's window-owning pipeline to double-write the swapchain.
        raster->SetPresentationBlitEnabled(presentationMode == PresentationMode::Direct);
        if (warnFwd && !warnedForwardPlusFallback) {
            warnedForwardPlusFallback = true;
            Log::Warn("RenderPipeline: Mode::ForwardPlus is not yet implemented; falling back to Forward.");
        }
        shadow = std::make_unique<eyelib::rendering::ShadowFeature>();
        sky    = std::make_unique<eyelib::rendering::SkyboxFeature>();
        editor = std::make_unique<eyelib::rendering::EditorFeature>();

        // Per-scene IBL probe swap: `SkyboxFeature` consults `RasterFeature` for the
        // bootstrap-env fallback when no cubemap is bound (procedural / none). Wiring the
        // pointer here keeps the dependency one-way (sky → raster) and avoids any
        // feature-to-feature lookup at frame time.
        sky->SetDefaultEnvProvider(raster.get());

        if (!world->TrySubscribeFeature(raster.get())) {
            Log::Error("RenderPipeline::Initialize: failed to subscribe RasterFeature.");
            return false;
        }
        if (!world->TrySubscribeFeature(shadow.get())) {
            Log::Error("RenderPipeline::Initialize: failed to subscribe ShadowFeature.");
            return false;
        }
        if (!world->TrySubscribeFeature(sky.get())) {
            Log::Error("RenderPipeline::Initialize: failed to subscribe SkyboxFeature.");
            return false;
        }
        if (!world->TrySubscribeFeature(editor.get())) {
            Log::Error("RenderPipeline::Initialize: failed to subscribe EditorFeature.");
            return false;
        }

        world->Initialize();
        return world->IsInitialized();
    }

    void TeardownWorldAndFeatures() {
        if (world && editor) world->TryUnsubscribeFeature(editor.get());
        if (world && sky)    world->TryUnsubscribeFeature(sky.get());
        if (world && raster) world->TryUnsubscribeFeature(raster.get());
        if (world && shadow) world->TryUnsubscribeFeature(shadow.get());
        if (world) world->Shutdown();
        editor.reset();
        sky.reset();
        raster.reset();
        shadow.reset();
        world.reset();
    }
};

RenderPipeline::RenderPipeline() : m_impl(std::make_unique<Impl>()) {}

RenderPipeline::~RenderPipeline() {
    Shutdown();
}

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept = default;
RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool RenderPipeline::Initialize(Window& window, uint32_t width, uint32_t height,
                                RenderBackend backend) {
    if (!m_impl) return false;

    if (backend != RenderBackend::Vulkan && backend != RenderBackend::Default) {
        if (!m_impl->warnedNonVulkanBackend) {
            m_impl->warnedNonVulkanBackend = true;
            Log::Warn("RenderPipeline::Initialize: only Vulkan backend is supported by the new render module; "
                            "request normalized to Vulkan.");
        }
    }

    void* glfwWindowHandle = window.GetNativeHandle();
    if (!glfwWindowHandle) {
        Log::Error("RenderPipeline::Initialize: window native handle is null.");
        return false;
    }

    auto device = eyelib::rendering::rhi::CreateVulkanDevice(glfwWindowHandle, width, height);
    if (!device) {
        Log::Error("RenderPipeline::Initialize: CreateVulkanDevice returned null.");
        return false;
    }
    m_impl->ownedDevice = std::move(device);
    m_impl->device = m_impl->ownedDevice.get();
    m_impl->ownsDevice = true;

    if (!m_impl->BuildWorldAndFeatures(width, height)) {
        Log::Error("RenderPipeline::Initialize: world/feature setup failed.");
        Shutdown();
        return false;
    }
    Log::InfoF("RenderPipeline::Initialize: success (vulkan, %ux%u, owns device).", width, height);
    return true;
}

bool RenderPipeline::InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height) {
    if (!m_impl) return false;
    if (!device) {
        Log::Error("RenderPipeline::InitializeWithSharedDevice: device is null.");
        return false;
    }
    m_impl->device = static_cast<eyelib::rendering::rhi::IDevice*>(device.Get());
    m_impl->ownsDevice = false;
    // Shared-device pipelines are offscreen producers by default; the caller (editor) is
    // the swapchain compositor. Callers may flip back to Direct via SetPresentationMode
    // before the first Render if a different shape is desired.
    m_impl->presentationMode = PresentationMode::Offscreen;

    if (!m_impl->BuildWorldAndFeatures(width, height)) {
        Log::Error("RenderPipeline::InitializeWithSharedDevice: world/feature setup failed.");
        Shutdown();
        return false;
    }
    Log::InfoF("RenderPipeline::InitializeWithSharedDevice: success (vulkan, %ux%u, borrowed device).", width, height);
    return true;
}

bool RenderPipeline::InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height,
                                                RenderPipeline* shareFrom) {
    if (!m_impl) return false;
    if (shareFrom && !m_impl->warnedSharedShareFromUnimplemented) {
        m_impl->warnedSharedShareFromUnimplemented = true;
        Log::Warn("RenderPipeline::InitializeWithSharedDevice(shareFrom): shader-cache sharing across pipelines "
                  "is not wired yet; each pipeline uses its own ShaderLibrary cache.");
    }
    return InitializeWithSharedDevice(device, width, height);
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    if (!m_impl || !m_impl->device) return;
    // Resize the pipeline's *render output* only (offscreen HDR + downstream pass RTs reallocate
    // lazily on `outputSize` mismatch). The window swapchain has its own separate contract
    // (`OnWindowResized`) — callers drive that explicitly when the actual window framebuffer
    // changes. Conflating the two would let editor panel-size changes (which call this every
    // frame with viewport-area dimensions) recreate the GLFW/Vulkan swapchain mid-frame and
    // break the acquire/present pairing.
    m_impl->outputWidth = width;
    m_impl->outputHeight = height;
    if (m_impl->world) {
        m_impl->world->SetOutputSize(width, height);
    }
}

void RenderPipeline::Shutdown() {
    if (!m_impl) return;

    // Drop the cache attachment first so the cache stops pushing material updates into a
    // pipeline whose features / world are about to be torn down.
    if (m_impl->attachedCache) {
        ResourceCache* cache = m_impl->attachedCache;
        DetachResourceCache(*cache);
    }

    m_impl->TeardownWorldAndFeatures();
    m_impl->device = nullptr;
    if (m_impl->ownsDevice) {
        m_impl->ownedDevice.reset();
    }
    m_impl->ownsDevice = false;
    m_impl->hasRenderedAtLeastOnce = false;
}

void RenderPipeline::SetMode(Mode mode) {
    if (!m_impl) return;
    m_impl->mode = mode;
    if (m_impl->raster) {
        bool warnFwd = false;
        m_impl->raster->SetRenderMode(ToInternalMode(mode, warnFwd));
        if (warnFwd && !m_impl->warnedForwardPlusFallback) {
            m_impl->warnedForwardPlusFallback = true;
            Log::Warn("RenderPipeline: Mode::ForwardPlus is not yet implemented; falling back to Forward.");
        }
    }
}

RenderPipeline::Mode RenderPipeline::GetMode() const {
    return m_impl ? m_impl->mode : Mode::Hybrid;
}

void RenderPipeline::SetPresentationMode(PresentationMode mode) {
    if (!m_impl) return;
    m_impl->presentationMode = mode;
    if (m_impl->raster) {
        m_impl->raster->SetPresentationBlitEnabled(mode == PresentationMode::Direct);
    }
}

RenderPipeline::PresentationMode RenderPipeline::GetPresentationMode() const {
    return m_impl ? m_impl->presentationMode : PresentationMode::Direct;
}

void RenderPipeline::SetSkyboxBrightness(float multiplier) {
    if (!m_impl) return;
    m_impl->skyboxBrightness = std::max(0.0f, multiplier);
    // Plumbing into the new module's IBL probes lands when the editor sky panel wires brightness (see RenderPipeline.h).
}

float RenderPipeline::GetSkyboxBrightness() const {
    return m_impl ? m_impl->skyboxBrightness : 1.0f;
}

void RenderPipeline::ReloadShaders() {
    if (!m_impl) return;
    if (!m_impl->warnedReloadShadersUnimplemented) {
        m_impl->warnedReloadShadersUnimplemented = true;
        Log::Warn("RenderPipeline::ReloadShaders: shader hot-reload is not wired through ShaderLibrary; request ignored.");
    }
}

void RenderPipeline::UpdateUploads() {
    // No-op in v1: the new module's RenderWorld::RunFrame performs upload + execute together.
    // The call stays on the API for legacy parity.
}

void RenderPipeline::BeginPresentFrame() {
    if (!m_impl || !m_impl->device) return;
    m_impl->device->BeginFrame();
}

void RenderPipeline::EndPresentFrame() {
    if (!m_impl || !m_impl->device) return;
    m_impl->device->EndFrame();
}

void RenderPipeline::Present() {
    if (!m_impl || !m_impl->device) return;
    m_impl->device->Present();
}

void RenderPipeline::OnWindowResized(uint32_t width, uint32_t height) {
    if (!m_impl || !m_impl->device) return;
    m_impl->device->OnWindowResized(width, height);
}

void RenderPipeline::ComposeSwapchain() {
    if (!m_impl || !m_impl->device) return;
    // Pass nullptr — IDevice::BlitToDefaultFramebuffer treats null source as the "clear +
    // overlay-only" path: clears the swapchain image, begins the swapchain render pass, and
    // invokes the post-blit callback (where ImGui's draw data is recorded). This is the
    // editor's single-swapchain-compose-after-N-pipeline-renders flow.
    m_impl->device->BlitToDefaultFramebuffer(nullptr);
}

void RenderPipeline::Render(const RenderScene& scene) {
    if (!m_impl || !m_impl->device || !m_impl->world) return;

    // Push lights into the new module's process-global SceneLighting bus before any
    // RunFrame work runs — features (raster + shadow) read from the bus during their
    // pass execution. M4.0a: directional + point + spot. Skybox/IBL still uses the
    // RasterFeature defaults until per-scene IBL probe swap lands upstream.
    PublishLightsToSceneLighting(scene);

    // Push per-scene sky source into the SkyboxFeature. Resolved through the friend-only
    // SkyboxAccess seam so this TU stays the only place that names the internal
    // `SkyboxRequest`. A null `scene.skybox` clears the feature back to `Kind::None`.
    if (m_impl->sky) {
        eyelib::rendering::SkyboxRequest req{};
        if (scene.skybox) {
            const auto resolved = SkyboxAccess::Resolve(*scene.skybox);
            switch (resolved.kind) {
                case Skybox::Kind::None:       req.kind = eyelib::rendering::SkyboxRequest::Kind::None;       break;
                case Skybox::Kind::Procedural: req.kind = eyelib::rendering::SkyboxRequest::Kind::Procedural; break;
                case Skybox::Kind::Cubemap:    req.kind = eyelib::rendering::SkyboxRequest::Kind::Cubemap;    break;
            }
            req.cubemap = resolved.cubemap;
        }
        req.brightness = (scene.skyboxBrightness < 0.0f) ? 0.0f : scene.skyboxBrightness;
        m_impl->sky->SetRequest(req);
    }

    // Repopulate the internal adapter from the public scene POD (camera + flat draw list).
    // The adapter buffers persist across frames (`vector::clear` keeps capacity).
    m_impl->sceneAdapter.Reset(scene);
    m_impl->world->SetSceneView(&m_impl->sceneAdapter);

    // Record-only: BeginFrame/EndFrame are caller-driven via BeginPresentFrame/EndPresentFrame.
    // Matches the legacy editor's contract (multi-pipeline single device frame) and works for
    // the standalone player too (player calls the frame-boundary methods around Render).
    auto* cmd = m_impl->device->GetActiveRecordingCommandList();
    m_impl->world->RunFrame(0.0f, cmd);

    // Offscreen pipelines are sampled by an external compositor (editor ImGui viewports). Keep
    // the end-of-frame contract explicit at the API boundary as a safety net.
    if (cmd && m_impl->presentationMode == PresentationMode::Offscreen) {
        if (auto* rt = m_impl->world->Blackboard().TryGetRenderTarget(eyelib::rendering::BlackboardSlot::MainHdrRenderTarget)) {
            if (auto* color0 = rt->GetColorTexture(0)) {
                cmd->EndDynamicRendering();
                m_impl->device->TransitionTextureToState(color0, eyelib::rendering::rhi::ResourceState::ShaderResource);
                m_impl->device->FlushBarrierTracker(cmd);
            }
        }
    }

    // Legacy single-frame contract: debug lines are consumed by exactly one Render call.
    // Clear here (after RunFrame has both scheduled and executed) so a caller that stops
    // feeding lines next frame does not see stale geometry.
    if (m_impl->editor) m_impl->editor->ClearLines();

    m_impl->hasRenderedAtLeastOnce = true;
}

void RenderPipeline::BlitToScreen() {
    // No-op in v1: RasterFeature already includes BlitMainHdrToDefaultPass in the frame graph.
    // The call stays on the API for legacy parity (editor/player explicit blit calls compile).
}

void RenderPipeline::RestoreDefaultFramebufferAndViewport(uint32_t width, uint32_t height) {
    (void)width; (void)height;
    // No-op in v1: the new module's frame graph leaves the swapchain bound after the final
    // blit pass; ImGui draws on top with its own viewport.
}

void RenderPipeline::Finish() {
    if (m_impl && m_impl->device) m_impl->device->Finish();
}

bool RenderPipeline::HasValidOutput() const {
    return m_impl && m_impl->device && m_impl->world && m_impl->world->IsInitialized()
           && m_impl->hasRenderedAtLeastOnce;
}

bool RenderPipeline::OwnsDevice() const {
    return m_impl && m_impl->ownsDevice;
}

RenderDevice RenderPipeline::GetDevice() const {
    if (!m_impl) return RenderDevice{};
    return RenderDevice{static_cast<void*>(m_impl->device)};
}

const char* RenderPipeline::GetBackendName() const {
    return (m_impl && m_impl->device) ? "vulkan" : nullptr;
}

RenderBackend RenderPipeline::GetBackend() const {
    return RenderBackend::Vulkan;
}

void* RenderPipeline::GetOutputColorTextureNativeHandle() const {
    if (!m_impl || !m_impl->world) return nullptr;
    auto* rt = m_impl->world->Blackboard().TryGetRenderTarget(eyelib::rendering::BlackboardSlot::MainHdrRenderTarget);
    if (!rt) return nullptr;
    auto* tex = rt->GetColorTexture(0);
    return tex ? tex->GetNativeTexture() : nullptr;
}

// =====================================================================================
// M1.7 — Debug hooks parity
// =====================================================================================

void* RenderPipeline::GetShadowDepthMapNativeViewForDebug() const {
    if (!m_impl || !m_impl->world) return nullptr;
    auto* rt = m_impl->world->Blackboard().TryGetRenderTarget(
        eyelib::rendering::BlackboardSlot::DirectionalShadowMapArray);
    if (!rt) return nullptr;
    auto* depth = rt->GetDepthTexture();
    return depth ? depth->GetNativeTexture() : nullptr;
}

uint32_t RenderPipeline::GetShadowCascadeCountForDebug() const {
    if (!m_impl || !m_impl->world) return 0u;
    // CSM cascade count is fixed at 3 in the new module's `ShadowFeature`
    // (`m_lightViewProj` array length). When shadows have not produced a render target
    // (pre-first-Render or shadows disabled), report 0 so the editor panel hides itself.
    auto* rt = m_impl->world->Blackboard().TryGetRenderTarget(
        eyelib::rendering::BlackboardSlot::DirectionalShadowMapArray);
    return rt ? 3u : 0u;
}

uint32_t RenderPipeline::GetShadowMapResolutionForDebug() const {
    if (!m_impl || !m_impl->world) return 0u;
    auto* rt = m_impl->world->Blackboard().TryGetRenderTarget(
        eyelib::rendering::BlackboardSlot::DirectionalShadowMapArray);
    return rt ? rt->GetWidth() : 0u;
}

std::vector<RenderPipeline::FrameGraphPassInfo> RenderPipeline::GetFrameGraphPasses() const {
    // v1: GpuProfiler exposes pipeline-statistics counters only, not per-pass timestamps.
    // Until timestamp queries are wired, ProfilerPanel can use CPU-side ImGui timing.
    return {};
}

void RenderPipeline::Render(const RenderScene& scene,
                            const std::vector<uint32_t>* entityIdsForOpaque,
                            const std::vector<uint32_t>* entityIdsForTransparent) {
    if (m_impl && (entityIdsForOpaque || entityIdsForTransparent) && !m_impl->warnedGpuPickingPaused) {
        m_impl->warnedGpuPickingPaused = true;
        Log::Warn("RenderPipeline: GPU picking is not implemented; entityIds arguments ignored.");
    }
    Render(scene);
}

bool RenderPipeline::ReadPixelEntityId(uint32_t /*x*/, uint32_t /*y*/, uint32_t* outId) const {
    if (outId) *outId = 0u;
    if (m_impl && !m_impl->warnedGpuPickingPaused) {
        m_impl->warnedGpuPickingPaused = true;
        Log::Warn("RenderPipeline::ReadPixelEntityId: GPU picking is not implemented; returning false.");
    }
    return false;
}

void RenderPipeline::RenderOutlineFromIdBuffer(uint32_t /*selectedPackedId*/,
                                               float /*r*/, float /*g*/, float /*b*/) {
    if (m_impl && !m_impl->warnedGpuPickingPaused) {
        m_impl->warnedGpuPickingPaused = true;
        Log::Warn("RenderPipeline::RenderOutlineFromIdBuffer: GPU picking is not implemented; call ignored.");
    }
}

// =====================================================================================
// Debug-line surface (live; legacy parity).
//
// The legacy `eyelib::api::RenderPipeline` exposed three entry points (immediate
// `RenderDebugLines` + two `SetPending*` variants). They all share a single internal
// destination: the next `Render(scene)` consumes them. The `view`/`proj` arguments are
// ignored because the new module builds the camera transform from `RenderScene::camera`
// and uploads it as `PerView`; the vertex shader reads `uPerView.uViewProjection`.
//
// Lines clear after each `Render(scene)` (see `Render`) so a caller that stops feeding
// the surface does not see stale geometry — preserving the legacy single-frame contract.
// =====================================================================================

namespace {

void FillDebugLineSpan(std::vector<eyelib::rendering::DebugLine>& out,
                      const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines,
                      const eyelib::math::Vec4& color) {
    out.clear();
    out.reserve(lines.size());
    for (const auto& seg : lines)
        out.push_back({seg.first, seg.second, color});
}

void FillDebugLineSpanColored(std::vector<eyelib::rendering::DebugLine>& out,
                             const std::vector<std::pair<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>, eyelib::math::Vec4>>& linesWithColors) {
    out.clear();
    out.reserve(linesWithColors.size());
    for (const auto& entry : linesWithColors)
        out.push_back({entry.first.first, entry.first.second, entry.second});
}

}  // namespace

void RenderPipeline::RenderDebugLines(
    const eyelib::math::Mat4& /*view*/, const eyelib::math::Mat4& /*proj*/,
    const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines,
    float r, float g, float b) {
    if (!m_impl || !m_impl->editor) return;
    std::vector<eyelib::rendering::DebugLine> internal;
    FillDebugLineSpan(internal, lines, {r, g, b, 1.0f});
    m_impl->editor->SetLines(internal);
}

void RenderPipeline::SetPendingDebugLines(
    const eyelib::math::Mat4& /*view*/, const eyelib::math::Mat4& /*proj*/,
    const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines,
    float r, float g, float b) {
    if (!m_impl || !m_impl->editor) return;
    std::vector<eyelib::rendering::DebugLine> internal;
    FillDebugLineSpan(internal, lines, {r, g, b, 1.0f});
    m_impl->editor->SetLines(internal);
}

void RenderPipeline::SetPendingDebugLinesColored(
    const eyelib::math::Mat4& /*view*/, const eyelib::math::Mat4& /*proj*/,
    const std::vector<std::pair<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>, eyelib::math::Vec4>>& linesWithColors) {
    if (!m_impl || !m_impl->editor) return;
    std::vector<eyelib::rendering::DebugLine> internal;
    FillDebugLineSpanColored(internal, linesWithColors);
    m_impl->editor->SetLines(internal);
}

// =====================================================================================
// M4.7 — ImGui-Vulkan adapter (live; not paused). Both methods delegate to the underlying
// `eyelib::rendering::rhi::IDevice`. Callers (editor `ScopedImGui` / player `RuntimeOverlay`)
// receive POD values they can hand directly to Dear ImGui's Vulkan backend.
// =====================================================================================

bool RenderPipeline::GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) const {
    if (!out) return false;
    if (!m_impl || !m_impl->device) return false;
    eyelib::rendering::rhi::ImGuiVulkanInitInfo internalInfo{};
    if (!m_impl->device->GetImGuiVulkanInitInfo(&internalInfo)) return false;
    out->instance        = internalInfo.instance;
    out->physical_device = internalInfo.physical_device;
    out->device          = internalInfo.device;
    out->queue_family    = internalInfo.queue_family;
    out->queue           = internalInfo.queue;
    out->pipeline_cache  = internalInfo.pipeline_cache;
    out->descriptor_pool = internalInfo.descriptor_pool;
    out->min_image_count = internalInfo.min_image_count;
    out->image_count     = internalInfo.image_count;
    out->render_pass     = internalInfo.render_pass;
    return true;
}

void RenderPipeline::SetPostBlitCallback(std::function<void(void*)> cb) {
    if (!m_impl || !m_impl->device) return;
    m_impl->device->SetPostBlitCallback(std::move(cb));
}

void* RenderPipeline::GetDefaultVulkanSampler() const {
    // The sealed-module discipline forbids adding `GetDefaultSampler` to the abstract
    // `eyelib::rendering::rhi::IDevice` interface (would be a `eyelib/rendering/` edit).
    // The façade impl lives outside that tree and is allowed to consult the Vulkan
    // concrete device directly — same pattern that `Initialize(Window&, ...)` uses to
    // call `CreateDeviceVulkan`.
    if (!m_impl || !m_impl->device) return nullptr;
    auto* vkDev = dynamic_cast<eyelib::rendering::rhi::DeviceVK*>(m_impl->device);
    if (!vkDev) return nullptr;
    return vkDev->GetDefaultSampler();
}

// =====================================================================================
// M1.8 — Output / readback / streaming / stereo / XR / fence parity (all paused stubs)
// =====================================================================================

void RenderPipeline::Resize(const RenderTarget& /*target*/) {
    if (m_impl && !m_impl->warnedRtResizePaused) {
        m_impl->warnedRtResizePaused = true;
        Log::Warn("RenderPipeline::Resize(const RenderTarget&): offscreen render-target resize is not implemented; "
                  "use Resize(width, height).");
    }
}

bool RenderPipeline::ReadOutputPixels(uint32_t /*width*/, uint32_t /*height*/, uint8_t* /*outRGBA*/) const {
    if (m_impl && !m_impl->warnedReadbackPaused) {
        m_impl->warnedReadbackPaused = true;
        Log::Warn("RenderPipeline::ReadOutputPixels: synchronous swapchain readback is not implemented.");
    }
    return false;
}

bool RenderPipeline::ReadPixelsFromRenderTarget(const RenderTarget& /*target*/, uint8_t* /*outRGBA*/) const {
    if (m_impl && !m_impl->warnedReadbackPaused) {
        m_impl->warnedReadbackPaused = true;
        Log::Warn("RenderPipeline::ReadPixelsFromRenderTarget: synchronous RT readback is not implemented.");
    }
    return false;
}

void* RenderPipeline::CreatePixelPackBuffer(size_t /*sizeInBytes*/) const {
    if (m_impl && !m_impl->warnedPboPaused) {
        m_impl->warnedPboPaused = true;
        Log::Warn("RenderPipeline::CreatePixelPackBuffer: async PBO readback path is not implemented.");
    }
    return nullptr;
}

void RenderPipeline::DestroyPixelPackBuffer(void* /*handle*/) const {
    // No warn — symmetric with Create which already warned. Safe on null.
}

void RenderPipeline::ReadPixelsToPBO(const RenderTarget& /*target*/, void* /*pboHandle*/) const {
    if (m_impl && !m_impl->warnedPboPaused) {
        m_impl->warnedPboPaused = true;
        Log::Warn("RenderPipeline::ReadPixelsToPBO: async PBO readback path is not implemented.");
    }
}

void RenderPipeline::MapPixelPackBuffer(void* /*pboHandle*/, void* outBuffer, size_t sizeInBytes,
                                        uint32_t /*width*/, uint32_t /*height*/) const {
    if (outBuffer && sizeInBytes > 0) std::memset(outBuffer, 0, sizeInBytes);
    if (m_impl && !m_impl->warnedPboPaused) {
        m_impl->warnedPboPaused = true;
        Log::Warn("RenderPipeline::MapPixelPackBuffer: async PBO mapping path is not implemented.");
    }
}

void RenderPipeline::MapPixelPackBufferRaw(void* /*pboHandle*/, void* outBuffer, size_t sizeInBytes) const {
    if (outBuffer && sizeInBytes > 0) std::memset(outBuffer, 0, sizeInBytes);
    if (m_impl && !m_impl->warnedPboPaused) {
        m_impl->warnedPboPaused = true;
        Log::Warn("RenderPipeline::MapPixelPackBufferRaw: async PBO mapping path is not implemented.");
    }
}

void RenderPipeline::RunStreamingNV12Pass(const RenderTarget& /*sourceRT*/, void* /*nv12PBOHandle*/,
                                          size_t /*nv12Size*/, uint32_t /*width*/, uint32_t /*height*/,
                                          uint32_t /*pitch*/) const {
    if (m_impl && !m_impl->warnedNv12Paused) {
        m_impl->warnedNv12Paused = true;
        Log::Warn("RenderPipeline::RunStreamingNV12Pass: NV12 streaming compute path is not implemented.");
    }
}

void RenderPipeline::RenderStereo(const RenderScene& scene,
                                  const CameraView& /*leftCam*/, const CameraView& /*rightCam*/) {
    if (m_impl && !m_impl->warnedStereoPaused) {
        m_impl->warnedStereoPaused = true;
        Log::Warn("RenderPipeline::RenderStereo: stereo/XR single-pass render is not implemented; "
                  "using mono Render(scene).");
    }
    Render(scene);
}

void RenderPipeline::BlitOutputTo(RenderTarget& /*target*/) {
    if (m_impl && !m_impl->warnedStereoPaused) {
        m_impl->warnedStereoPaused = true;
        Log::Warn("RenderPipeline::BlitOutputTo: eye-target blit is not implemented; call ignored.");
    }
}

void RenderPipeline::BlitStereoToScreen(RenderTarget& /*left*/, RenderTarget& /*right*/,
                                        uint32_t /*totalWidth*/, uint32_t /*totalHeight*/) {
    if (m_impl && !m_impl->warnedStereoPaused) {
        m_impl->warnedStereoPaused = true;
        Log::Warn("RenderPipeline::BlitStereoToScreen: stereo preview blit is not implemented; call ignored.");
    }
}

void* RenderPipeline::InsertFrameFence() {
    if (m_impl && !m_impl->warnedFencePaused) {
        m_impl->warnedFencePaused = true;
        Log::Warn("RenderPipeline::InsertFrameFence: explicit GPU fence API is not implemented; returning null.");
    }
    return nullptr;
}

void RenderPipeline::WaitFence(void* /*fence*/) {
    // Symmetric with Insert; safe on null.
}

void RenderPipeline::WaitAndDestroyFence(void* /*fence*/) {
    // Symmetric with Insert; safe on null.
}

// =====================================================================================
// M3.7b — Resource cache integration
// =====================================================================================

void RenderPipeline::AttachResourceCache(ResourceCache& cache) {
    if (!m_impl) return;

    // Re-attach is allowed but must detach the previous cache (different instance).
    if (m_impl->attachedCache && m_impl->attachedCache != &cache) {
        DetachResourceCache(*m_impl->attachedCache);
    }
    m_impl->attachedCache = &cache;

    // Wire the cache as the mesh resolver for raster + shadow features. The cache's
    // resolver instance lives as long as the cache itself; the features hold a borrowed
    // pointer (per `IMeshHandleResolver` contract — non-owning).
    auto* resolver = ResourceCacheAccess::GetMeshResolver(cache);
    if (m_impl->raster) m_impl->raster->SetMeshResolver(resolver);
    if (m_impl->shadow) m_impl->shadow->SetMeshResolver(resolver);

    // Register this pipeline with the cache so subsequent material updates fan out here.
    // The cache primes our MaterialTable with all currently-known records during this call.
    ResourceCacheAccess::RegisterPipeline(cache, this);
}

void RenderPipeline::DetachResourceCache(ResourceCache& cache) {
    if (!m_impl) return;
    if (m_impl->attachedCache != &cache) return;

    ResourceCacheAccess::UnregisterPipeline(cache, this);
    if (m_impl->raster) m_impl->raster->SetMeshResolver(nullptr);
    if (m_impl->shadow) m_impl->shadow->SetMeshResolver(nullptr);
    m_impl->attachedCache = nullptr;
}

void RenderPipeline::SetMaterialRecord(uint32_t materialId, const MaterialRecord& record) {
    if (!m_impl || !m_impl->world) return;
    auto& table = m_impl->world->Resources().MaterialTable();
    table.SetMaterial(materialId, ToInternalRecord(record));
}

} // namespace rendering
} // namespace api
} // namespace eyelib
