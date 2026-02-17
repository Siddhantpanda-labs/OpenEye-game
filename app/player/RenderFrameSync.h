#pragma once

#include "Overlay.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace openeye::player {

/** Explicit slot state for debug validation. */
enum class FrameSlotState {
    Free,       /**< Available for game thread in GetFillSlot. */
    Filling,    /**< Game thread is filling the slot (between GetFillSlot and SubmitFrame). */
    Submitted,  /**< Submitted; render thread may acquire it. */
    Rendering,  /**< Render thread has acquired and is drawing; will call ReleaseFrame. */
};

/**
 * N-buffered frame-in-flight sync: game thread fills slots, render thread acquires/draws/releases.
 * Each slot has a GPU fence; ReleaseFrame(slot, newFence) waits on the slot's previous fence then stores newFence.
 * So CPU slot reuse is gated on GPU completion (no lifetime hazards).
 *
 * Use SetFenceWaiter to provide wait+destroy for fences (e.g. renderBridge.WaitAndDestroyFence).
 */
class RenderFrameSync {
public:
    /** Number of frame slots (2 = double buffer, 3 = triple buffer). */
    static constexpr int kBufferCount = 3;

    /** Set callback to wait for and destroy a fence (e.g. [&rb](void* f) { rb.WaitAndDestroyFence(f); }). Must be set before using ReleaseFrame(slot, fence). */
    void SetFenceWaiter(std::function<void(void*)> waiter);

    /** Called from game thread: wait until a slot is available, return its index. Fill GetSceneBuffer(slot) then SubmitFrame(slot, ...). */
    int GetFillSlot();

    /** Scene buffer for the given slot. Game thread fills the slot returned by GetFillSlot(); render thread reads in AcquireFrame. */
    eyelib::api::RenderScene& GetSceneBuffer(int slot);

    /** Frame allocator for the given slot (use for per-frame snapshot data). */
    api::FrameAllocator* GetFrameAllocator(int slot);

    /** Called from game thread after filling the slot. Blocks until the previous submission was consumed (so we don't overwrite too soon). */
    void SubmitFrame(int slot, uint32_t width, uint32_t height, const OverlaySnapshot& overlay);

    /** Phase 5: Game thread only. Enqueue a Resize command; render thread will apply it before the next frame. */
    void EnqueueResize(uint32_t width, uint32_t height);

    /** Phase 5: Render thread only. Returns and clears all pending Resize commands (apply in order; typically coalesce to latest). */
    std::vector<std::pair<uint32_t, uint32_t>> TakeResizeCommands();

    /** Payload handed to the render thread. */
    struct AcquiredFrame {
        int slot = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        OverlaySnapshot overlay;
    };

    /** Called from render thread: blocks until a frame is submitted (or shutdown). Returns nullopt when shutdown. */
    std::optional<AcquiredFrame> AcquireFrame();

    /** Called from render thread when done drawing. Pass the fence from InsertFrameFence() (or nullptr if no draw). Waits on slot's previous fence then stores \a newFence; allows game thread to reuse the slot only after GPU is done with the previous use. */
    void ReleaseFrame(int slot, void* newFence = nullptr);

    /** Signal shutdown so AcquireFrame() returns nullopt. Call before joining the render thread. */
    void SignalShutdown();

private:
    struct FrameSlot {
        api::FrameAllocator allocator;
        eyelib::api::RenderScene scene;
        /** Fence for the frame last submitted for this slot. Waited on in ReleaseFrame(slot, newFence) before reusing. */
        void* pendingFence = nullptr;

        FrameSlot() : allocator(1024 * 1024) {}

        /** Zero scene pointers/counts and reset allocator. Does not touch pendingFence. Destroys Drawables before reset so shared_ptr refcounts are correct. */
        void Reset() {
            if (scene.opaque && scene.opaqueCount > 0) {
                for (size_t i = 0; i < scene.opaqueCount; ++i)
                    scene.opaque[i].~Drawable();
            }
            if (scene.transparent && scene.transparentCount > 0) {
                for (size_t i = 0; i < scene.transparentCount; ++i)
                    scene.transparent[i].~Drawable();
            }
            scene.opaque = nullptr;
            scene.opaqueCount = 0;
            scene.transparent = nullptr;
            scene.transparentCount = 0;
            scene.pointLights = nullptr;
            scene.pointLightCount = 0;
            scene.spotLights = nullptr;
            scene.spotLightCount = 0;
            allocator.Reset();
        }
    };

    std::array<FrameSlot, kBufferCount> m_slots;
    std::array<bool, kBufferCount> m_slotReleased{true, true, true};
    std::array<FrameSlotState, kBufferCount> m_slotState{FrameSlotState::Free, FrameSlotState::Free, FrameSlotState::Free};
    int m_nextFillSlot = 0;
    std::function<void(void*)> m_fenceWaiter;

    bool m_payloadReady = false;
    int m_payloadSlot = 0;
    uint32_t m_payloadWidth = 0;
    uint32_t m_payloadHeight = 0;
    OverlaySnapshot m_payloadOverlay;

    /** Release-acquire handoff: game store-release after filling payload; render load-acquire before reading scene. */
    std::atomic<uintptr_t> m_sceneReadyFrameId{0};

    std::atomic<bool> m_shutdown{false};
    std::mutex m_mutex;
    std::condition_variable m_cvPayloadReady;   /**< Signalled when a frame is submitted; render thread waits. */
    std::condition_variable m_cvSlotReleased; /**< Signalled when a slot is released; game thread waits. */

    std::vector<std::pair<uint32_t, uint32_t>> m_resizeQueue;
};

} // namespace openeye::player
