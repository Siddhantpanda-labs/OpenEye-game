#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

void BlitMainHdrToDefaultPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(m_slot);
    if (!rt)
        return;
    if (rhi::ITexture* color = rt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::ShaderResource);
}

void BlitMainHdrToDefaultPass::Execute(rhi::ICommandList* cmd) {
    if (!m_device || !m_blackboard || !cmd)
        return;
    cmd->EndDynamicRendering();
    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_slot);
    if (!rt)
        return;
    m_device->BlitToDefaultFramebuffer(rt);
}

}  // namespace rendering
}  // namespace eyelib
