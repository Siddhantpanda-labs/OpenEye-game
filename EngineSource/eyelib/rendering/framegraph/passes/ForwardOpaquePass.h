#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"

namespace eyelib {
namespace rendering {

struct DrawList;
class IMeshHandleResolver;

namespace rhi {
struct IDevice;
struct ICommandList;
struct IPipeline;
struct IDescriptorSet;
}

/** HDR color + shared depth: binds PerFrame/PerView + PerObject SSBO + directional light, draws visible `DrawList` instances. */
class ForwardOpaquePass final : public IFrameGraphPass {
public:
    ForwardOpaquePass(FrameGraphBlackboard* blackboard, BlackboardSlot hdrSlot, rhi::IDevice* device, rhi::IPipeline* pipeline,
                      const IMeshHandleResolver* meshResolver, const DrawList* drawList) noexcept;
    ~ForwardOpaquePass() override;

    const char* DebugName() const noexcept override { return pass_names::kForwardOpaque; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    BlackboardSlot m_hdrSlot = BlackboardSlot::MainHdrRenderTarget;
    rhi::IDevice* m_device = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    const DrawList* m_drawList = nullptr;

    rhi::IDescriptorSet* m_frameSet = nullptr;
    uint64_t m_frameSetBuffersKey = 0;

    rhi::IDescriptorSet* m_forwardDataSet = nullptr;
    uint64_t m_forwardDataBuffersKey = 0;

    bool m_diagLoggedEarlyExit = false;
    bool m_diagLoggedNoRt = false;
    bool m_diagLoggedBuffersMissing = false;
    bool m_diagLoggedNoDraws = false;
    bool m_diagLoggedFirstDraw = false;
};

}  // namespace rendering
}  // namespace eyelib
