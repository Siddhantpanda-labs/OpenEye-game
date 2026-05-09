#pragma once

#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/rhi/RHI.h"
#include <array>

namespace eyelib {
namespace rendering {

class ShadowBlurPass final : public IFrameGraphPass {
public:
    ShadowBlurPass();
    ~ShadowBlurPass() override;

    void Setup(rhi::IDevice* device, rhi::IRenderTarget* sourceRt, rhi::IRenderTarget* scratchRt);

    const char* DebugName() const noexcept override { return "ShadowBlurPass"; }
    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    rhi::IDevice* m_device = nullptr;
    rhi::IRenderTarget* m_sourceRt = nullptr;
    rhi::IRenderTarget* m_scratchRt = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;
    rhi::IDescriptorSet* m_descriptorSetH = nullptr;
    rhi::IDescriptorSet* m_descriptorSetV = nullptr;
};

} // namespace rendering
} // namespace eyelib
