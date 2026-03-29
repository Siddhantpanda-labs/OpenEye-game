#pragma once

namespace eyelib {
namespace render {

/** Which frame-graph wiring path is active; used with executor override maps. */
enum class FrameGraphExecutorSet : uint32_t {
    Deferred = 0,
    Forward = 1,
    ForwardPlus = 2,
    ForwardStereo = 3,
};

}  // namespace render
}  // namespace eyelib
