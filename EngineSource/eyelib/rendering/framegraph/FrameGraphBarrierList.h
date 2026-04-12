#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {
struct ITexture;
}

/**
 * Per-pass texture transitions accumulated before `FlushBarrierTracker` (M4).
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
};

}  // namespace rendering
}  // namespace eyelib
