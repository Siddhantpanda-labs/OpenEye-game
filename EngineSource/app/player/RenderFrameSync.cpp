#include "RenderFrameSync.h"

#include <cassert>

namespace openeye::player {

int RenderFrameSync::GetFillSlot() {
    std::unique_lock lock(m_mutex);
    m_cvSlotReleased.wait(lock, [this] {
        return m_shutdown.load(std::memory_order_relaxed) || m_slotReleased[m_nextFillSlot];
    });
    if (m_shutdown.load(std::memory_order_relaxed)) {
        return -1;
    }
    const int slot = m_nextFillSlot;
    assert(slot >= 0 && slot < kBufferCount);
    assert(m_slotState[static_cast<size_t>(slot)] == FrameSlotState::Free);
    m_slotState[static_cast<size_t>(slot)] = FrameSlotState::Filling;
    m_slotReleased[slot] = false;
    m_nextFillSlot = (m_nextFillSlot + 1) % kBufferCount;
    return slot;
}

void RenderFrameSync::SetFenceWaiter(std::function<void(void*)> waiter) {
    m_fenceWaiter = std::move(waiter);
}

eyelib::api::RenderScene& RenderFrameSync::GetSceneBuffer(int slot) {
    assert(slot >= 0 && slot < kBufferCount);
    return m_slots[static_cast<size_t>(slot)].scene;
}

api::FrameAllocator* RenderFrameSync::GetFrameAllocator(int slot) {
    assert(slot >= 0 && slot < kBufferCount);
    return &m_slots[static_cast<size_t>(slot)].allocator;
}

void RenderFrameSync::SubmitFrame(int slot, uint32_t width, uint32_t height, const OverlaySnapshot& overlay) {
    assert(slot >= 0 && slot < kBufferCount);
    std::unique_lock lock(m_mutex);
    assert(m_slotState[static_cast<size_t>(slot)] == FrameSlotState::Filling);
    m_slotState[static_cast<size_t>(slot)] = FrameSlotState::Submitted;
    m_payloadSlot = slot;
    m_payloadWidth = width;
    m_payloadHeight = height;
    m_payloadOverlay = overlay;
    m_sceneReadyFrameId.store(static_cast<uintptr_t>(slot), std::memory_order_release);
    m_payloadReady = true;
    m_cvPayloadReady.notify_one();
    // Back-pressure: wait until the render thread releases this slot.
    m_cvSlotReleased.wait(lock, [this, slot] {
        return m_shutdown.load(std::memory_order_relaxed) || m_slotReleased[static_cast<size_t>(slot)];
    });
}

void RenderFrameSync::EnqueueResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    std::lock_guard lock(m_mutex);
    m_resizeQueue.emplace_back(width, height);
}

std::vector<std::pair<uint32_t, uint32_t>> RenderFrameSync::TakeResizeCommands() {
    std::lock_guard lock(m_mutex);
    std::vector<std::pair<uint32_t, uint32_t>> out;
    out.swap(m_resizeQueue);
    return out;
}

std::optional<RenderFrameSync::AcquiredFrame> RenderFrameSync::AcquireFrame() {
    std::unique_lock lock(m_mutex);
    m_cvPayloadReady.wait(lock, [this] { return m_shutdown.load(std::memory_order_relaxed) || m_payloadReady; });
    if (m_shutdown.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }
    const int slot = m_payloadSlot;
    assert(slot >= 0 && slot < kBufferCount);
    assert(m_slotState[static_cast<size_t>(slot)] == FrameSlotState::Submitted);
    m_slotState[static_cast<size_t>(slot)] = FrameSlotState::Rendering;
    AcquiredFrame frame;
    frame.slot = slot;
    frame.width = m_payloadWidth;
    frame.height = m_payloadHeight;
    frame.overlay = m_payloadOverlay;
    (void)m_sceneReadyFrameId.load(std::memory_order_acquire);
    m_payloadReady = false;
    return frame;
}

void RenderFrameSync::ReleaseFrame(int slot, void* newFence) {
    assert(slot >= 0 && slot < kBufferCount);
    {
        std::lock_guard lock(m_mutex);
        // Rendering: threaded path (AcquireFrame set it). Filling: single-threaded path (we never Submit/Acquire).
        const auto state = m_slotState[static_cast<size_t>(slot)];
        assert(state == FrameSlotState::Rendering || state == FrameSlotState::Filling);
        m_slotState[static_cast<size_t>(slot)] = FrameSlotState::Free;
    }
    FrameSlot& s = m_slots[static_cast<size_t>(slot)];
    if (s.pendingFence && m_fenceWaiter) {
        m_fenceWaiter(s.pendingFence);
        s.pendingFence = nullptr;
    }
    s.pendingFence = newFence;
    s.Reset();
    std::lock_guard lock(m_mutex);
    m_slotReleased[static_cast<size_t>(slot)] = true;
    m_cvSlotReleased.notify_one();
}

void RenderFrameSync::SignalShutdown() {
    m_shutdown.store(true, std::memory_order_relaxed);
    if (m_fenceWaiter) {
        for (size_t i = 0; i < kBufferCount; ++i) {
            if (m_slots[i].pendingFence) {
                m_fenceWaiter(m_slots[i].pendingFence);
                m_slots[i].pendingFence = nullptr;
            }
        }
    }
    m_cvPayloadReady.notify_all();
    m_cvSlotReleased.notify_all();
}

} // namespace openeye::player
