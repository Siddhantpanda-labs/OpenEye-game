#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include <array>
#include <string>

namespace eyelib {
namespace rendering {

/**
 * Registration-time ownership of blackboard slots (doc §3 — catch double-writers before runtime).
 * `TryReserve` succeeds on first claim or when the same owner re-registers.
 */
class BlackboardWriterRegistry {
public:
    static constexpr size_t kSlotCount = static_cast<size_t>(BlackboardSlot::SlotCount);

    void Clear() noexcept {
        for (auto& o : m_owners)
            o.clear();
    }

    bool TryReserve(BlackboardSlot slot, const char* ownerName) {
        if (!ownerName)
            return false;
        const size_t i = static_cast<size_t>(slot);
        if (i >= kSlotCount)
            return false;
        if (m_owners[i].empty()) {
            m_owners[i] = ownerName;
            return true;
        }
        return m_owners[i] == ownerName;
    }

    /** If `slot` is reserved by `ownerName`, clears it (runtime unsubscribe). No-op if wrong owner. */
    bool TryRelease(BlackboardSlot slot, const char* ownerName) noexcept {
        if (!ownerName)
            return false;
        const size_t i = static_cast<size_t>(slot);
        if (i >= kSlotCount)
            return false;
        if (m_owners[i] != ownerName)
            return false;
        m_owners[i].clear();
        return true;
    }

    bool HasReserved(BlackboardSlot slot) const noexcept {
        const size_t i = static_cast<size_t>(slot);
        if (i >= kSlotCount)
            return false;
        return !m_owners[i].empty();
    }

    const std::string& Owner(BlackboardSlot slot) const noexcept {
        static const std::string kEmpty;
        const size_t i = static_cast<size_t>(slot);
        if (i >= kSlotCount)
            return kEmpty;
        return m_owners[i];
    }

    /** Publish is allowed only when the slot was reserved and the publisher name matches. */
    bool IsPublisherAllowed(BlackboardSlot slot, const char* publisherName) const noexcept {
        if (!publisherName)
            return false;
        const size_t i = static_cast<size_t>(slot);
        if (i >= kSlotCount)
            return false;
        if (m_owners[i].empty())
            return false;
        return m_owners[i] == publisherName;
    }

private:
    std::array<std::string, kSlotCount> m_owners{};
};

}  // namespace rendering
}  // namespace eyelib
