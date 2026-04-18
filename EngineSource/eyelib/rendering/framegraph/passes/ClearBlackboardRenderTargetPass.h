#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

struct FrameGraphBarrierList;

/** Clears the render target currently published on a blackboard slot (typically HDR). */
class ClearBlackboardRenderTargetPass final : public IFrameGraphPass {
public:
    ClearBlackboardRenderTargetPass(FrameGraphBlackboard* blackboard, BlackboardSlot slot, rhi::ClearValue clear) noexcept
        : m_blackboard(blackboard)
        , m_slot(slot)
        , m_clear(clear) {}

    const char* DebugName() const noexcept override { return pass_names::kClearBlackboardRenderTarget; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;

    void Execute(rhi::ICommandList* cmd) override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    BlackboardSlot m_slot = BlackboardSlot::MainHdrRenderTarget;
    rhi::ClearValue m_clear{};
};

}  // namespace rendering
}  // namespace eyelib
