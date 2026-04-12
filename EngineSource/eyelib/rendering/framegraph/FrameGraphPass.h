#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {
struct ICommandList;
}

class FrameGraphBlackboard;
struct FrameGraphBarrierList;

/** One unit of GPU work in the frame graph (doc §1). */
class IFrameGraphPass {
public:
    virtual ~IFrameGraphPass() = default;

    virtual const char* DebugName() const noexcept = 0;

    /** Invoked after `FrameGraph::Compile` assigns stable indices (barrier scheduling / validation). */
    virtual void OnCompiled(uint32_t passIndexInGraph) noexcept { (void)passIndexInGraph; }

    /**
     * Declares texture layout transitions applied before `Execute` for this pass when a barrier tracker exists.
     * Default: none. Order: collect → `IDevice::TransitionTextureToState` → `FlushBarrierTracker` → `Execute`.
     */
    virtual void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
        (void)blackboard;
        (void)out;
    }

    /** Invoked during graph execution; must not retain `cmd` beyond this call (doc §9). */
    virtual void Execute(rhi::ICommandList* cmd) = 0;
};

}  // namespace rendering
}  // namespace eyelib
