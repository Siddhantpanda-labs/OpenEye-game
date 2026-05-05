#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {

/** Per-frame CPU context (monotonic index, timing). No RHI types. */
struct FrameContext {
    uint64_t frameIndex = 0;
    float deltaSeconds = 0.0f;
};

}  // namespace rendering
}  // namespace eyelib
