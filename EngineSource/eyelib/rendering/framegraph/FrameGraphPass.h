#pragma once

#include <cstdint>
#include "eyelib/rendering/framegraph/FrameGraphAttachmentOps.h"

namespace eyelib {
namespace rendering {
namespace rhi {
struct ICommandList;
}

class FrameGraphBlackboard;
struct FrameGraphBarrierList;
namespace rhi { struct IRenderTarget; }

/** One unit of GPU work in the frame graph (doc §1). */
class IFrameGraphPass {
public:
    virtual ~IFrameGraphPass() = default;

    virtual const char* DebugName() const noexcept = 0;

    /** Invoked after `FrameGraph::Compile` assigns stable indices (barrier scheduling / validation). */
    virtual void OnCompiled(uint32_t passIndexInGraph) noexcept { (void)passIndexInGraph; }

    /**
     * Declares texture layout transitions for this pass. Collected during `FrameGraph::Compile` (with blackboard),
     * deduplicated, then replayed before `Execute`. Default: none.
     */
    virtual void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
        (void)blackboard;
        (void)out;
    }

    /**
     * Optional pass-level attachment ops contract for the render target this pass begins.
     * Return true only when this pass is responsible for begin-rendering ops on `outRt`.
     */
    virtual bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                             RenderAttachmentOps& outOps) const {
        (void)blackboard;
        outRt = nullptr;
        (void)outOps;
        return false;
    }

    /** Invoked during graph execution; must not retain `cmd` beyond this call (doc §9). */
    virtual void Execute(rhi::ICommandList* cmd) = 0;
};

}  // namespace rendering
}  // namespace eyelib
