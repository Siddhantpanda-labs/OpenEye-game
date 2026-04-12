#include "eyelib/rendering/framegraph/passes/ClearBlackboardRenderTargetPass.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {

void ClearBlackboardRenderTargetPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(m_slot);
    if (!rt)
        return;
    if (rhi::ITexture* color = rt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
    if (rhi::ITexture* depth = rt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
}

void ClearBlackboardRenderTargetPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard)
        return;
    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_slot);
    if (!rt)
        return;
    cmd->SetRenderTarget(rt);
    cmd->ClearRenderTarget(rt, m_clear);
}

}  // namespace rendering
}  // namespace eyelib
