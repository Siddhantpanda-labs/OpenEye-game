#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"

namespace eyelib {
namespace rendering {
namespace rhi {
struct ICommandList;
struct IPipeline;
}

class FrameGraphBlackboard;
struct FrameGraphBarrierList;

/**
 * Fullscreen draw into a blackboard HDR target (bootstrap for deferred-style compose).
 * Expects `IPipeline` built for the same color/depth formats as `MainHdrRenderTarget` (e.g. RGBA16F + D24S8).
 */
class FullscreenHdrComposePass final : public IFrameGraphPass {
public:
    FullscreenHdrComposePass(FrameGraphBlackboard* blackboard, BlackboardSlot slot, rhi::IPipeline* pipeline) noexcept
        : m_blackboard(blackboard)
        , m_slot(slot)
        , m_pipeline(pipeline) {}

    const char* DebugName() const noexcept override { return "FullscreenHdrCompose"; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    BlackboardSlot m_slot = BlackboardSlot::MainHdrRenderTarget;
    rhi::IPipeline* m_pipeline = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
