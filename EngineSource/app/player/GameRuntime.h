#pragma once

#include "AssetBinding.h"
#include "PlayerConfig.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"
#include "WorldAudio.h"

#include "api/asset/All.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"
#include "api/render/RenderBackend.h"
#include "api/world/All.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace eyelib { namespace api { class Window; } }

#ifdef OPENEYE_BUILD_STREAMING
#include "app/streaming/VRStreamingServer.h"
#endif

namespace openeye::player {

/**
 * Single-threaded game runtime for embedding (e.g. editor Game view) and standalone player.
 * Initialize with world, asset registry, roots; optional window (null = headless); then Tick(dt)
 * and Render(w, h) each frame. GetOutputColorTextureNativeHandle() returns the pipeline output for ImGui.
 * No render thread; overlay is drawn by the host after Render().
 *
 * ECS render snapshot capture: GameRuntime wires WorldRenderBridge::BindWorldSnapshotCapture on Initialize
 * and UnbindWorldSnapshotCapture on Shutdown. Hosts (Player, Editor, AI Playground) must not call
 * World::SetCaptureSnapshotCallback for the play GameRuntime path. Edit-style hosts that skip Tick should call
 * RefreshRenderSnapshotFromWorld() before Render() so the snapshot matches the live world.
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

    /**
     * Standalone present path: RHI BeginFrame once per frame before Vulkan host UI and before Render().
     * Pairs with EndFrame inside pipeline BlitToScreen. No-op when headless or embedded (shared device).
     */
    void BeginPresentFrame();

    /**
     * When set, Render() replaces the scene camera from BuildScene with this raw view (same convention as editor:
     * BuildFreeFlyCameraView / world camera). RenderPipeline adjusts per backend once — do not pre-adjust here.
     */
    void SetFrameCameraOverride(const eyelib::api::CameraView* view);

    /** Build scene from world and render to pipeline. No-op when headless; returns null. */
    void* Render(uint32_t width, uint32_t height);

    /** Pipeline output texture handle (ImTextureID). Valid after Render(); null when headless or not ready. */
    void* GetOutputColorTextureNativeHandle() const;

    /** RHI device (for overlay Vulkan init). Valid when not headless after Initialize(). */
    eyelib::api::RenderDevice GetDevice() const;

    /** Reload shaders (e.g. after ShaderWatcher copy). No-op when headless. */
    void ReloadShaders();

    /**
     * Rebuild physics, animation, scripting, and audio bridges from the current world without recreating the GPU device.
     * Call after World::LoadScene (e.g. restoring a play-session snapshot on Stop). No-op when not ready.
     */
    void ResyncFromWorld();

    /**
     * Refill the ECS render snapshot from the current world (FlushTransforms + CaptureRenderSnapshot).
     * Call each frame before Render() when not using Tick() (e.g. AI Playground edit mode) so the viewport matches chat/tools.
     * No-op when headless or not ready. When simulation runs via Tick(), the scheduler callback already captures — skip this to avoid redundant work.
     */
    void RefreshRenderSnapshotFromWorld();

#ifdef OPENEYE_BUILD_STREAMING
    /** Start VR streaming server (discovery + stream port). No-op when headless. */
    bool StartStreaming(uint16_t streamPort = 27016);
    void StopStreaming();
    bool IsStreaming() const;
    /** Number of connected VR clients (0 when not streaming). */
    size_t GetStreamingClientCount() const;
    /** Streaming FPS (frames sent/sec). 0 when not streaming. */
    float GetStreamingFPS() const;
    /** Callback (yaw, pitch, roll in radians) when client sends head tracking. Set before StartStreaming to drive XR camera. */
    void SetStreamingGyroCallback(std::function<void(float yaw, float pitch, float roll)> cb);
#endif

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
    bool m_frameCameraOverrideActive = false;
    eyelib::api::CameraView m_frameCameraOverride{};
    AssetBindStats m_assetBindStats{};

#ifdef OPENEYE_BUILD_STREAMING
    std::unique_ptr<eyelib::app::VRStreamingServer> m_streaming;
    std::function<void(float yaw, float pitch, float roll)> m_streamingGyroCallback;
    std::vector<uint8_t> m_streamingReadbackBuffer;

    // Pipelined frame queue: 5 slots; when full, drop oldest so encoder always gets newest.
    // Render thread never waits on encoder; avoids back-pressure on encode spikes.
    static constexpr int kStreamingFrameQueueSize = 5;
    std::mutex m_streamingEncodeMutex;
    std::condition_variable m_streamingEncodeCV;
    std::queue<int> m_streamingToEncode;
    std::queue<int> m_streamingNV12ToEncode;
    std::deque<int> m_streamingFreeList;
    std::array<std::vector<uint8_t>, kStreamingFrameQueueSize> m_streamingPool;
    std::thread m_streamingEncoderThread;
    std::atomic<bool> m_streamingEncoderStop{false};
    void StartStreamingEncoderThread();
    void StopStreamingEncoderThread();
    void StreamingEncoderThreadLoop();
#endif
};

} // namespace openeye::player
