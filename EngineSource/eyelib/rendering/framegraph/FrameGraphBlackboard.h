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
}

/**
 * Per-frame shared resources between passes (doc §3).
 * Debug: two different writers in one frame assert; optional registry enforces registration-time owners.
 */
class FrameGraphBlackboard {
public:
    FrameGraphBlackboard() = default;

    /** When non-null, `PublishRenderTarget` must use a writer registered via `BlackboardWriterRegistry::TryReserve`. */
    void SetWriterRegistry(const BlackboardWriterRegistry* registry) noexcept { m_writerRegistry = registry; }

    void ClearFrame() noexcept;

    void PublishRenderTarget(BlackboardSlot slot, const char* writerName, rhi::IRenderTarget* rt);

    rhi::IRenderTarget* TryGetRenderTarget(BlackboardSlot slot) const noexcept;

private:
    static constexpr size_t kSlotCount = static_cast<size_t>(BlackboardSlot::SlotCount);

    const BlackboardWriterRegistry* m_writerRegistry = nullptr;
    std::array<rhi::IRenderTarget*, kSlotCount> m_targets{};
    std::array<std::string, kSlotCount> m_writers{};
    std::array<bool, kSlotCount> m_used{};
};

inline void FrameGraphBlackboard::ClearFrame() noexcept {
    m_targets.fill(nullptr);
    for (auto& w : m_writers)
        w.clear();
    m_used.fill(false);
}

inline void FrameGraphBlackboard::PublishRenderTarget(BlackboardSlot slot, const char* writerName, rhi::IRenderTarget* rt) {
    const size_t i = static_cast<size_t>(slot);
    if (i >= kSlotCount)
        return;
#if !defined(NDEBUG)
    if (m_used[i] && writerName && !m_writers[i].empty() && m_writers[i] != writerName) {
        assert(false && "FrameGraphBlackboard: two different writers published the same slot this frame");
    }
    if (m_writerRegistry && writerName) {
        assert(m_writerRegistry->IsPublisherAllowed(slot, writerName) && "Blackboard: publisher not registered for this slot");
    }
#endif
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

}  // namespace rendering
}  // namespace eyelib
