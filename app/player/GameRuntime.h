#pragma once

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
 * Single-threaded game runtime for embedding (e.g. editor Game view).
 * Initialize with world, asset registry, roots, window and size; then Tick(dt) and Render(w, h)
 * each frame. GetOutputColorTextureNativeHandle() returns the pipeline output for ImGui.
 * No render thread, no overlay.
 */
class GameRuntime {
public:
    GameRuntime() = default;
    ~GameRuntime();

    GameRuntime(const GameRuntime&) = delete;
    GameRuntime& operator=(const GameRuntime&) = delete;

    /** Initialize physics, render, animation, scripting, audio; build from world; bind assets.
     * When sharedDevice (e.g. from pipeline.GetDevice()) is valid, render uses that device (Model A).
     * When sharedDevice is null (default), render creates its own device from window. */
    bool Initialize(eyelib::api::world::World& world,
                    eyelib::api::AssetRegistry& assetRegistry,
                    const std::vector<std::string>& assetRoots,
                    eyelib::api::Window& window,
                    uint32_t width, uint32_t height,
                    eyelib::render::RenderBackend backend = eyelib::render::RenderBackend::Default,
                    eyelib::api::RenderDevice sharedDevice = eyelib::api::RenderDevice());

    void Shutdown();

    bool IsReady() const { return m_ready; }

    /** Advance simulation: physics step, sync to world, animation, audio, scripting, world update. */
    void Tick(float deltaSeconds);

    /** Build scene from world and render to pipeline. Call after Tick. Resizes if (width, height) changed. Returns output texture handle. */
    void* Render(uint32_t width, uint32_t height);

    /** Pipeline output texture handle (ImTextureID). Valid after Render(); null if not ready. */
    void* GetOutputColorTextureNativeHandle() const;

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
};

} // namespace openeye::player
