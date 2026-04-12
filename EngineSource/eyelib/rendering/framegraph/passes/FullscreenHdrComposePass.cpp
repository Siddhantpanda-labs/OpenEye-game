#include "eyelib/rendering/framegraph/passes/FullscreenHdrComposePass.h"

#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

void FullscreenHdrComposePass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(m_slot);
    if (!rt)
        return;
    if (rhi::ITexture* color = rt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
    if (rhi::ITexture* depth = rt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
}

void FullscreenHdrComposePass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline)
        return;
    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_slot);
    if (!rt)
        return;

    const float tint[4] = {0.18f, 0.42f, 0.62f, 1.0f};
    cmd->SetRenderTarget(rt);
    cmd->SetPushConstants(0, sizeof(tint), tint);
    cmd->BindPipeline(m_pipeline);
    cmd->DrawFullscreenQuad(m_pipeline);
}

}  // namespace rendering
}  // namespace eyelib
