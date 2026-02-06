#pragma once

#include "Overlay.h"
#include "api/core/FrameAllocator.h"
#include "api/render/All.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace openeye::player {

/**
 * Double-buffered sync between game thread (fills scene + submits) and render thread (acquires, draws, releases).
 * Game thread must not touch GL after submitting; render thread owns GL and consumes frames.
 *
 * Phase 3: One FrameAllocator per slot. GetFillSlot() resets the allocator for that slot.
 * Phase 5: Game thread enqueues Resize(w,h); render thread drains commands then renders.
 */
class RenderFrameSync {
public:
    static constexpr int kBufferCount = 2;

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

    /** Called from render thread when done drawing this frame. Allows the game thread to reuse the slot. */
    void ReleaseFrame(int slot);

    /** Signal shutdown so AcquireFrame() returns nullopt. Call before joining the render thread. */
    void SignalShutdown();

private:
    struct FrameSlot {
        api::FrameAllocator allocator;
        eyelib::api::RenderScene scene;

        FrameSlot() : allocator(1024 * 1024), scene(&allocator) {}

        void Reset() {
            allocator.Reset();
            scene = eyelib::api::RenderScene(&allocator);
        }
    };

    std::array<FrameSlot, kBufferCount> m_slots;
    std::array<bool, kBufferCount> m_slotReleased{true, true};
    int m_nextFillSlot = 0;

    bool m_payloadReady = false;
    int m_payloadSlot = 0;
    uint32_t m_payloadWidth = 0;
    uint32_t m_payloadHeight = 0;
    OverlaySnapshot m_payloadOverlay;

    std::atomic<bool> m_shutdown{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::vector<std::pair<uint32_t, uint32_t>> m_resizeQueue;
};

} // namespace openeye::player
