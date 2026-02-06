#include "RenderFrameSync.h"

namespace openeye::player {

int RenderFrameSync::GetFillSlot() {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] {
        return m_shutdown.load(std::memory_order_relaxed) || m_slotReleased[m_nextFillSlot];
    });
    if (m_shutdown.load(std::memory_order_relaxed)) {
        return -1;
    }
    m_slotReleased[m_nextFillSlot] = false;
    const int slot = m_nextFillSlot;
    m_nextFillSlot = 1 - m_nextFillSlot;
    m_slots[static_cast<size_t>(slot)].Reset();
    return slot;
}

eyelib::api::RenderScene& RenderFrameSync::GetSceneBuffer(int slot) {
    return m_slots[static_cast<size_t>(slot)].scene;
}

api::FrameAllocator* RenderFrameSync::GetFrameAllocator(int slot) {
    return &m_slots[static_cast<size_t>(slot)].allocator;
}

void RenderFrameSync::SubmitFrame(int slot, uint32_t width, uint32_t height, const OverlaySnapshot& overlay) {
    std::unique_lock lock(m_mutex);
    m_payloadSlot = slot;
    m_payloadWidth = width;
    m_payloadHeight = height;
    m_payloadOverlay = overlay;
    m_payloadReady = true;
    m_cv.notify_all();
    // Back-pressure: wait until the render thread releases this slot.
    m_cv.wait(lock, [this, slot] {
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
    m_cv.wait(lock, [this] { return m_shutdown.load(std::memory_order_relaxed) || m_payloadReady; });
    if (m_shutdown.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }
    AcquiredFrame frame;
    frame.slot = m_payloadSlot;
    frame.width = m_payloadWidth;
    frame.height = m_payloadHeight;
    frame.overlay = m_payloadOverlay;
    m_payloadReady = false;
    m_cv.notify_all();
    return frame;
}

void RenderFrameSync::ReleaseFrame(int slot) {
    std::lock_guard lock(m_mutex);
    m_slotReleased[static_cast<size_t>(slot)] = true;
    m_cv.notify_all();
}

void RenderFrameSync::SignalShutdown() {
    m_shutdown.store(true, std::memory_order_relaxed);
    m_cv.notify_all();
}

} // namespace openeye::player
