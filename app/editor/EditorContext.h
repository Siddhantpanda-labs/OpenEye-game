#pragma once

#include "api/world/All.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib { namespace api { class RenderPipeline; class AssetRegistry; class Window; } }
namespace openeye { namespace player { class GameRuntime; } }

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

    /** True when the scene root (name from scene file) is selected in hierarchy; then Inspector shows skybox. */
    bool sceneSelected = false;

    /** Camera for scene view (viewport). Independent of game camera. */
    EditorCamera editorCamera;

    /** Phase 4b: scene view render. Set by EditorApp; null if not available. */
    eyelib::api::RenderPipeline* sceneViewPipeline = nullptr;
    eyelib::api::AssetRegistry* assetRegistry = nullptr;
    std::vector<std::string> assetRoots;

    /** Set by EditorApp; used for Game view (Play mode). */
    eyelib::api::Window* window = nullptr;

    /** Play mode: game runtime drives Game view. Null when not playing. */
    bool isPlaying = false;
    std::unique_ptr<openeye::player::GameRuntime> gameRuntime;
    /** Set when Stop is clicked; runtime is destroyed at start of next frame to avoid GL shutdown mid-frame crash. */
    bool gameRuntimeStopRequested = false;

    /** Path to snapshot saved when Play was pressed; used to revert world on Stop. Empty when no snapshot. */
    std::string playSnapshotPath;

    enum class Modal { None, Open, SaveAs };
    Modal activeModal = Modal::None;
    char pathBuffer[512] = "scenes/main.json";
};

} // namespace openeye::editor
