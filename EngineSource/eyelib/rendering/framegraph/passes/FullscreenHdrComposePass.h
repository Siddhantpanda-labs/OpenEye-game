#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"

namespace eyelib {
namespace rendering {
namespace rhi {
struct ICommandList;
struct IDevice;
struct IDescriptorSet;
struct IPipeline;
}

class FrameGraphBlackboard;

/**
 * Fullscreen draw into a blackboard HDR target (bootstrap for deferred-style compose).
 * Expects `IPipeline` built for the same color/depth formats as `MainHdrRenderTarget` (e.g. RGBA16F + D24S8).
 * Binds Frame set (PerFrame + PerView UBOs) when both buffers are published on the blackboard.
 * Color/depth layouts for drawing are established by an earlier clear pass + `SetRenderTarget` (no duplicate `CollectTextureBarriers` here).
 */
class FullscreenHdrComposePass final : public IFrameGraphPass {
public:
    FullscreenHdrComposePass(FrameGraphBlackboard* blackboard, BlackboardSlot slot, rhi::IDevice* device, rhi::IPipeline* pipeline) noexcept;
    ~FullscreenHdrComposePass() override;

    const char* DebugName() const noexcept override { return pass_names::kFullscreenHdrCompose; }

    void Execute(rhi::ICommandList* cmd) override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    BlackboardSlot m_slot = BlackboardSlot::MainHdrRenderTarget;
    rhi::IDevice* m_device = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;
    rhi::IDescriptorSet* m_frameSets[3] = {nullptr, nullptr, nullptr};
    uint64_t m_frameSetBuffersKeys[3] = {0, 0, 0};
};

}  // namespace rendering
}  // namespace eyelib
