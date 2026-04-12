#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {

/** Typed visibility / draw submission list (CPU). Populated by VisibilitySystem. */
struct DrawList {
    /** Summary count for scheduling / debug; kept in sync with `placeholderIndices` in stub path. */
    uint32_t visibleCount = 0;
    std::vector<uint32_t> placeholderIndices;

    void Clear() noexcept {
        visibleCount = 0;
        placeholderIndices.clear();
    }
};

}  // namespace rendering
}  // namespace eyelib
