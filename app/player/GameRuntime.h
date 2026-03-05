#pragma once

#include "AssetBinding.h"
#include "PlayerConfig.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"
#include "WorldAudio.h"

#include "api/assets/All.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"
#include "api/world/All.h"

#include <cstdint>
#include <vector>

namespace eyelib { namespace api { class Window; } }

namespace openeye::player {

/**
 * Single-threaded game runtime for embedding (e.g. editor Game view) and standalone player.
 * Initialize with world, asset registry, roots; optional window (null = headless); then Tick(dt)
 * and Render(w, h) each frame. GetOutputColorTextureNativeHandle() returns the pipeline output for ImGui.
 * No render thread; overlay is drawn by the host after Render().
 */
class GameRuntime {
public:
    GameRuntime() = default;
    ~GameRuntime();

    GameRuntime(const GameRuntime&) = delete;
    GameRuntime& operator=(const GameRuntime&) = delete;

    /** Initialize physics, animation, scripting, audio; optionally render when window is non-null.
     * When window is null (headless), only Tick() is valid; Render() is a no-op.
     * When sharedDevice is valid (e.g. editor), render uses that device; otherwise creates from window.
     * options: when non-null, fixedTimestep/maxSubsteps etc. are applied (standalone player). */
    bool Initialize(eyelib::api::world::World& world,
                    eyelib::api::AssetRegistry& assetRegistry,
                    const std::vector<std::string>& assetRoots,
                    eyelib::api::Window* window,
                    uint32_t width, uint32_t height,
                    eyelib::render::RenderBackend backend = eyelib::render::RenderBackend::Default,
                    eyelib::api::RenderDevice sharedDevice = eyelib::api::RenderDevice(),
                    const RuntimeOptions* options = nullptr);

    void Shutdown();

    bool IsReady() const { return m_ready; }
    bool IsHeadless() const { return !m_render; }

    /** Last BindAssets result (for logging / strict-asset check). Valid after Initialize(). */
    AssetBindStats GetAssetBindingStats() const { return m_assetBindStats; }

    /** Advance simulation: physics step, sync to world, animation, audio, scripting, world update. */
    void Tick(float deltaSeconds);

    /** Build scene from world and render to pipeline. Call after Tick. No-op when headless; returns null. */
    void* Render(uint32_t width, uint32_t height);

    /** Pipeline output texture handle (ImTextureID). Valid after Render(); null when headless or not ready. */
    void* GetOutputColorTextureNativeHandle() const;

    /** Reload shaders (e.g. after ShaderWatcher copy). No-op when headless. */
    void ReloadShaders();

private:
    void ResetScene();

    eyelib::api::world::World* m_world = nullptr;
    eyelib::api::AssetRegistry* m_assetRegistry = nullptr;
    std::vector<std::string> m_assetRoots;

    RuntimeOptions m_options;
    std::unique_ptr<WorldRenderBridge> m_render;
    std::unique_ptr<WorldPhysicsBridge> m_physics;
    std::unique_ptr<WorldAnimationBridge> m_animation;
    std::unique_ptr<WorldScriptingBridge> m_scripting;
    std::unique_ptr<WorldAudioBridge> m_audio;

    eyelib::api::RenderScene m_scene;
    api::FrameAllocator m_frameAlloc{1024 * 1024};
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_ready = false;
    AssetBindStats m_assetBindStats{};
};

} // namespace openeye::player
