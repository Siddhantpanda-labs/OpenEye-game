#pragma once

#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"

namespace eyelib {
namespace rendering {

class FrameGraphBlackboard;

namespace rhi {
struct IDevice;
struct IPipeline;
struct IDescriptorSet;
}

/**
 * Deferred shading lighting pass.
 * Reads G-buffer MRTs, **G-buffer depth** (same depth target `GBufferPass` wrote), and DirLight UBO; writes HDR color on
 * `MainHdrRenderTarget`. Binds `PipelineLayoutKind::RasterDeferredLighting`.
 */
class DeferredLightingPass final : public IFrameGraphPass {
public:
    DeferredLightingPass(FrameGraphBlackboard* blackboard,
                         rhi::IDevice* device,
                         rhi::IPipeline* pipeline) noexcept;

    ~DeferredLightingPass();

    const char* DebugName() const noexcept override { return "DeferredLightingPass"; }

    /** Update frame-varying pointers so this persistent pass can be reused each frame without reallocation. */
    void Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline) noexcept;

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    rhi::IDevice* m_device = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;

    rhi::IDescriptorSet* m_frameSets[3] = {nullptr, nullptr, nullptr};
    uint64_t m_frameSetKeys[3] = {0, 0, 0};

    rhi::IDescriptorSet* m_passSets[3] = {nullptr, nullptr, nullptr};
    uint64_t m_passSetKeys[3] = {0, 0, 0};
};

} // namespace rendering
} // namespace eyelib
