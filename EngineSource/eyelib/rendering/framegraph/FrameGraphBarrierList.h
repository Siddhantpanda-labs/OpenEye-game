#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <utility>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {
struct ITexture;
}

/**
 * Per-pass texture transitions accumulated before `FlushBarrierTracker` (M4).
 * Filled during `FrameGraph::Compile`; replayed in `Execute` (A.6).
 */
struct FrameGraphBarrierList {
    struct Entry {
        rhi::ITexture* texture = nullptr;
        rhi::ResourceState toState = rhi::ResourceState::Undefined;
    };
    std::vector<Entry> entries;

    void Clear() noexcept { entries.clear(); }

    void Push(rhi::ITexture* texture, rhi::ResourceState toState) {
        if (texture)
            entries.push_back({texture, toState});
    }

    /** Remove consecutive redundant transitions (same texture, same target state). */
    void DedupeConsecutiveSameTargetState() noexcept {
        if (entries.size() < 2)
            return;
        std::vector<Entry> compact;
        compact.reserve(entries.size());
        for (const Entry& e : entries) {
            if (!e.texture)
                continue;
            if (!compact.empty() && compact.back().texture == e.texture && compact.back().toState == e.toState)
                continue;
            compact.push_back(e);
        }
        entries = std::move(compact);
    }
};

}  // namespace rendering
}  // namespace eyelib
