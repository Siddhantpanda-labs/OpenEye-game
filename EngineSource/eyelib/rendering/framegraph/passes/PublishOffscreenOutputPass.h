#pragma once

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

/**
 * Finalising barrier-only pass for **offscreen** raster pipelines (no in-graph swapchain blit).
 *
 * Contract: when a pipeline produces an offscreen HDR output that an external compositor (e.g.
 * the editor sampling viewports through ImGui) will consume as a sampled image, the pipeline
 * **must** leave the output color texture in `rhi::ResourceState::ShaderResource` at end of frame.
 * In the `Direct` (swapchain-owning) shape this happens implicitly inside `BlitMainHdrToDefaultPass`
 * (whose own `CollectTextureBarriers` transitions the HDR color to `ShaderResource` so the blit can
 * sample it). In the `Offscreen` shape, there is no blit pass, so `RasterFeature` appends this pass
 * as the last graph node to materialise the same end-state.
 *
 * `Execute` is a no-op — the work this pass does happens purely through `CollectTextureBarriers`,
 * which the framegraph compiler folds into the final merged barrier for the texture.
 * (`FrameGraph::Execute` ends dynamic rendering after each pass so barriers never run while an
 * attachment is still bound by `vkCmdBeginRendering`.)
 */
class PublishOffscreenOutputPass final : public IFrameGraphPass {
public:
    explicit PublishOffscreenOutputPass(BlackboardSlot slot = BlackboardSlot::MainHdrRenderTarget) noexcept
        : m_slot(slot) {}

    const char* DebugName() const noexcept override { return pass_names::kPublishOffscreenOutput; }

    void Reconfigure(BlackboardSlot slot) noexcept { m_slot = slot; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override {
        rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(m_slot);
        if (!rt)
            return;
        if (rhi::ITexture* color = rt->GetColorTexture(0))
            out.Push(color, rhi::ResourceState::ShaderResource);
    }

    void Execute(rhi::ICommandList* cmd) override { (void)cmd; }

private:
    BlackboardSlot m_slot = BlackboardSlot::MainHdrRenderTarget;
};

}  // namespace rendering
}  // namespace eyelib
