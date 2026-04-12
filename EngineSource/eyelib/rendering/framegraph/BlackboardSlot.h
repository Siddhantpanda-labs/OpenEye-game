#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {

/** Typed blackboard slots (extend as the module grows). */
enum class BlackboardSlot : uint8_t {
    /** Main HDR color + depth render target owned by `RenderResources`. */
    MainHdrRenderTarget = 0,
    SlotCount
};

}  // namespace rendering
}  // namespace eyelib
