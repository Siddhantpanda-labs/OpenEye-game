#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"

namespace eyelib {
namespace rendering {
namespace rhi {
struct IDevice;
}

/** Blits the main HDR target (blackboard) to the swapchain via `IDevice::BlitToDefaultFramebuffer`. */
class BlitMainHdrToDefaultPass final : public IFrameGraphPass {
public:
    BlitMainHdrToDefaultPass(rhi::IDevice* device, FrameGraphBlackboard* blackboard, BlackboardSlot slot = BlackboardSlot::MainHdrRenderTarget) noexcept
        : m_device(device)
        , m_blackboard(blackboard)
        , m_slot(slot) {}

    const char* DebugName() const noexcept override { return pass_names::kBlitMainHdrToDefault; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    rhi::IDevice* m_device = nullptr;
    FrameGraphBlackboard* m_blackboard = nullptr;
    BlackboardSlot m_slot = BlackboardSlot::MainHdrRenderTarget;
};

}  // namespace rendering
}  // namespace eyelib
