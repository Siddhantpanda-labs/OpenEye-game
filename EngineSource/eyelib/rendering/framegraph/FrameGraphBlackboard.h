#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/BlackboardWriterRegistry.h"
#include <array>
#include <cassert>
#include <string>

namespace eyelib {
namespace rendering {
namespace rhi {
struct IRenderTarget;
struct ITexture;
struct IBuffer;
}

/**
 * Per-frame shared resources between passes (doc §3).
 *
 * Each `BlackboardSlot` carries exactly one resource kind (render target, texture, or buffer). A slot
 * is written by a single writer per frame; mismatched publishes assert in debug. When a writer registry
 * is bound, publishing is further restricted to the slot's registered owner.
 */
class FrameGraphBlackboard {
public:
    FrameGraphBlackboard() = default;

    /** When non-null, `Publish*` must use a writer registered via `BlackboardWriterRegistry::TryReserve`. */
    void SetWriterRegistry(const BlackboardWriterRegistry* registry) noexcept { m_writerRegistry = registry; }

    void ClearFrame() noexcept;

    void PublishRenderTarget(BlackboardSlot slot, const char* writerName, rhi::IRenderTarget* rt);
    rhi::IRenderTarget* TryGetRenderTarget(BlackboardSlot slot) const noexcept;

    void PublishTexture(BlackboardSlot slot, const char* writerName, rhi::ITexture* tex);
    rhi::ITexture* TryGetTexture(BlackboardSlot slot) const noexcept;

    void PublishBuffer(BlackboardSlot slot, const char* writerName, rhi::IBuffer* buf);
    rhi::IBuffer* TryGetBuffer(BlackboardSlot slot) const noexcept;

private:
    static constexpr size_t kSlotCount = static_cast<size_t>(BlackboardSlot::SlotCount);

    void ValidateWriter(size_t slotIndex, const char* writerName) const;

    const BlackboardWriterRegistry* m_writerRegistry = nullptr;
    std::array<rhi::IRenderTarget*, kSlotCount> m_targets{};
    std::array<rhi::ITexture*, kSlotCount> m_textures{};
    std::array<rhi::IBuffer*, kSlotCount> m_buffers{};
    std::array<std::string, kSlotCount> m_writers{};
    std::array<bool, kSlotCount> m_used{};
};

inline void FrameGraphBlackboard::ClearFrame() noexcept {
    m_targets.fill(nullptr);
    m_textures.fill(nullptr);
    m_buffers.fill(nullptr);
    for (auto& w : m_writers)
        w.clear();
    m_used.fill(false);
}

inline void FrameGraphBlackboard::ValidateWriter(size_t slotIndex, const char* writerName) const {
#if !defined(NDEBUG)
    if (m_used[slotIndex] && writerName && !m_writers[slotIndex].empty() && m_writers[slotIndex] != writerName) {
        assert(false && "FrameGraphBlackboard: two different writers published the same slot this frame");
    }
    if (m_writerRegistry && writerName) {
        assert(m_writerRegistry->IsPublisherAllowed(static_cast<BlackboardSlot>(slotIndex), writerName) &&
               "Blackboard: publisher not registered for this slot");
    }
#else
    (void)slotIndex;
    (void)writerName;
#endif
}

inline void FrameGraphBlackboard::PublishRenderTarget(BlackboardSlot slot, const char* writerName, rhi::IRenderTarget* rt) {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return;
    ValidateWriter(i, writerName);
    m_targets[i] = rt;
    if (writerName)
        m_writers[i] = writerName;
    else if (!m_used[i])
        m_writers[i].clear();
    m_used[i] = true;
}

inline rhi::IRenderTarget* FrameGraphBlackboard::TryGetRenderTarget(BlackboardSlot slot) const noexcept {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return nullptr;
    return m_targets[i];
}

inline void FrameGraphBlackboard::PublishTexture(BlackboardSlot slot, const char* writerName, rhi::ITexture* tex) {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return;
    ValidateWriter(i, writerName);
    m_textures[i] = tex;
    if (writerName)
        m_writers[i] = writerName;
    else if (!m_used[i])
        m_writers[i].clear();
    m_used[i] = true;
}

inline rhi::ITexture* FrameGraphBlackboard::TryGetTexture(BlackboardSlot slot) const noexcept {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return nullptr;
    return m_textures[i];
}

inline void FrameGraphBlackboard::PublishBuffer(BlackboardSlot slot, const char* writerName, rhi::IBuffer* buf) {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return;
    ValidateWriter(i, writerName);
    m_buffers[i] = buf;
    if (writerName)
        m_writers[i] = writerName;
    else if (!m_used[i])
        m_writers[i].clear();
    m_used[i] = true;
}

inline rhi::IBuffer* FrameGraphBlackboard::TryGetBuffer(BlackboardSlot slot) const noexcept {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return nullptr;
    return m_buffers[i];
}

}  // namespace rendering
}  // namespace eyelib
