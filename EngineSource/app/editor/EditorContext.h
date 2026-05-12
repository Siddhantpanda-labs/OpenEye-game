#pragma once

#include "api/world/All.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib { namespace api { class AssetRegistry; class Material; class RenderPipeline; class Window; } }
namespace eyelib { namespace api { namespace rendering { class RenderPipeline; class ResourceCache; } } }
namespace openeye { namespace player {
class GameRuntime;
class RendererGPUResourceCacheNew;   // runtime-shared resource resolver (api/rendering)
class BuiltinResourceManagerNew;     // runtime-shared builtin primitive vendor (api/rendering)
class SkyboxCacheNew;                // runtime-shared per-scene skybox cache (api/rendering)
} }

namespace openeye::editor {

/** Editor camera for scene view (free-fly). WASD + Q/E move, right-drag rotates. */
struct EditorCamera {
    float position[3] = {0.0f, 0.0f, 10.0f};
    float yaw = 0.0f;           // radians (around world Y)
    float pitch = 0.0f;         // radians (look up/down)
    float fovDegrees = 60.0f;
    float moveSpeed = 8.0f;     // units per second
    float mouseSensitivity = 0.002f;
};

/**
 * Shared editor state: world (in-memory scene), current scene file path, selection.
 * All mutations go through context.world (API only).
 */
struct EditorContext {
    EditorContext();
    ~EditorContext();
    EditorContext(const EditorContext&) = delete;
    EditorContext& operator=(const EditorContext&) = delete;
    EditorContext(EditorContext&&) = default;
    EditorContext& operator=(EditorContext&&) = default;
    eyelib::api::world::World world;
    std::string scenePath;  // empty if never loaded/saved

    /** Selected entity in hierarchy; InvalidEntity when none selected. */
    ::eyelib::world::EntityId selectedEntity = ::eyelib::world::InvalidEntity;

    /** When non-empty, Inspector shows material editor for this asset (path or id). Set by Asset panel when a .mat is selected. */
    std::string selectedMaterialAssetId;

    /** True when the scene root (name from scene file) is selected in hierarchy; then Inspector shows skybox. */
    bool sceneSelected = false;

    /** Camera for scene view (viewport). Independent of game camera. */
    EditorCamera editorCamera;

    /** Scene view: when true, entity gizmo uses local space; when false, world space. */
    bool sceneGizmoLocal = false;

    /** Phase 4b: scene view render. Set by EditorApp; null if not available. */
    eyelib::api::RenderPipeline* sceneViewPipeline = nullptr;
    /**
     * Parallel scene-view pipeline on the new render module (`api/rendering`). Coexists with
     * `sceneViewPipeline` during the editor migration. When non-null, `SceneViewPanel` drives
     * this instead of the legacy `sceneViewPipeline`. Stays null through the M2/M3 dual-stack
     * window — the runtime flip (legacy → new) lands as a single edit at M3 closeout once
     * ImGui-Vulkan + game view + material preview can move to the new RHI device together.
     * Lifetime owned by `EditorApp`.
     */
    eyelib::api::rendering::RenderPipeline* sceneViewPipelineNew = nullptr;
    /** Game panel debug preview (when not playing). Set by EditorApp; null if not available. */
    eyelib::api::RenderPipeline* gameViewPipeline = nullptr;
    /**
     * Parallel game-view debug preview pipeline on `api/rendering`. Same lifetime / activation
     * model as `sceneViewPipelineNew`: stays null through M2/M3 dual-stack, becomes non-null at
     * the M3.9 device flip. When set, `GameViewPanel` non-playing branch uses it; the
     * playing-mode branch still routes through `OpenEyeGameRuntime` (closed in M4).
     */
    eyelib::api::rendering::RenderPipeline* gameViewPipelineNew = nullptr;
    /** Material Inspector sphere preview (shared device with scene view). */
    eyelib::api::RenderPipeline* materialPreviewPipeline = nullptr;
    /**
     * Parallel material preview pipeline on `api/rendering`. Same lifetime / activation model
     * as `sceneViewPipelineNew`: stays null through M2/M3 dual-stack, becomes non-null at the
     * M3.9 device flip. Drives the inspector sphere preview when non-null.
     */
    eyelib::api::rendering::RenderPipeline* materialPreviewPipelineNew = nullptr;
    /**
     * M3.7 — `api/rendering`-typed resource cache vending `Mesh / Material / Texture2D /
     * Shader` by `AssetId`. Parallel to the legacy `EditorGPUResourceCache` plugged into
     * `assetRegistry->SetGPUResolver` (which keeps vending legacy resources for the
     * legacy pipelines). Lifetime owned by `EditorApp`. The cache itself is a passive
     * cache of GPU-resident resources — descriptor→POD→`CreateMesh/CreateTexture/
     * CreateMaterial` translation runs through `gpuResolverNew` (below).
     */
    eyelib::api::rendering::ResourceCache* resourceCacheNew = nullptr;
    /**
     * M3.7 — runtime-shared resource resolver fronting `resourceCacheNew`. On miss, fetches
     * the legacy descriptor from `AssetRegistry`, translates it to a public POD, and uploads
     * via `ResourceCache::Create*`. Same class the player uses; the editor borrows it from
     * `OpenEyeGameRuntime`. Lifetime owned by `EditorApp`.
     */
    openeye::player::RendererGPUResourceCacheNew* gpuResolverNew = nullptr;
    /**
     * M3.7 — runtime-shared builtin-primitive vendor (`builtin://Cube`, `builtin://Sphere`,
     * …) on top of `resourceCacheNew`. Also vends the shared default material used as a
     * fallback for entities that don't reference one. Lifetime owned by `EditorApp`.
     */
    openeye::player::BuiltinResourceManagerNew* builtinManagerNew = nullptr;
    /**
     * Per-scene skybox cache: resolves the world's `skyboxAssetId` to a public
     * `api::rendering::Skybox` (procedural / cross / six-face) once and reuses it across
     * frames. Wired into `RenderScene::skybox` by the scene + game view panels so the
     * new `SkyboxFeature` overlay runs without leaking sky-loader logic into the panels.
     * Same cache is also handed to `GameRuntime` on Play so the live session reuses the
     * cubemap textures the editor preview already uploaded against this RHI device.
     * Lifetime owned by `EditorApp` inside the same scope as `gpuResolverNew` so cubemap
     * GPU textures are destroyed before the RHI device is torn down.
     *
     * Type lives in the `openeye::player` namespace because both editor preview and
     * editor-embedded Play need the same cache semantics — see SkyboxCacheNew.h.
     */
    openeye::player::SkyboxCacheNew* skyboxCacheNew = nullptr;
    /** When valid and not playing, Game panel shows this camera's view (debug). InvalidEntity = no preview. */
    ::eyelib::world::EntityId gameViewDebugCamera = ::eyelib::world::InvalidEntity;
    eyelib::api::AssetRegistry* assetRegistry = nullptr;
    /** Default material for primitives/untextured meshes. Set once at init (EditorApp) so scene builder does not lazy-create. */
    std::shared_ptr<eyelib::api::Material> defaultMaterial;
    /** Asset search roots; when a project is open this is { projectAssetsRoot }. */
    std::vector<std::string> assetRoots;

    /** Project-centric: path to project folder (e.g. Projects/MyWorld). Empty when no project open. */
    std::string projectPath;
    /** Project name from .openeye descriptor. */
    std::string projectName;
    /** Resolved path to project Assets folder (projectPath + "/" + assets from descriptor). */
    std::string projectAssetsRoot;
    /** Resolved path to project Cache folder. */
    std::string projectCacheRoot;

    /** Set by EditorApp; used for Game view (Play mode). */
    eyelib::api::Window* window = nullptr;
    /** Backend-specific ImGui texture id resolver (e.g. Vulkan descriptor set registration). */
    std::function<void*(void*)> resolveImGuiTextureId;

    /** Play mode: game runtime drives Game view. Null when not playing. */
    bool isPlaying = false;
    std::unique_ptr<openeye::player::GameRuntime> gameRuntime;
    /** Set when Stop is clicked; runtime is destroyed at start of next frame to avoid GL shutdown mid-frame crash. */
    bool gameRuntimeStopRequested = false;

    /** Path to snapshot saved when Play was pressed; used to revert world on Stop. Empty when no snapshot. */
    std::string playSnapshotPath;

    /** Engine assets path (e.g. Engine/assets). Set by EditorApp; used when closing project to restore assetRoots. */
    std::string engineAssetsPath;

    enum class Modal { None, Open, SaveAs, CreateProject };
    Modal activeModal = Modal::None;
    char pathBuffer[512] = "scenes/main.json";
    char projectPathBuffer[512] = "Projects/MyWorld";
    char projectNameBuffer[128] = "NewProject";

    /**
     * Work that mutates GPU-bound editor state (e.g. pipeline mode on view pipelines). Enqueue from UI during the frame;
     * EditorApp drains this immediately before device BeginFrame() so it never runs mid-command-buffer.
     */
    void EnqueueGpuSafeWork(std::function<void()>&& fn);
    void DrainGpuSafeWork();

private:
    std::vector<std::function<void()>> m_pendingGpuSafeWork;
};

} // namespace openeye::editor
