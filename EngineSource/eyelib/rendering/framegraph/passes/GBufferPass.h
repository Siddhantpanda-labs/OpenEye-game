#pragma once

#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"

namespace eyelib {
namespace rendering {

class FrameGraphBlackboard;
struct DrawList;
class IMeshHandleResolver;

namespace rhi {
struct IDevice;
struct IPipeline;
struct IDescriptorSet;
}

/**
 * Deferred Shading: G-Buffer Pass.
 * Binds `PipelineLayoutKind::RasterForwardMinimal` (reused since it just needs PerObject SSBO).
 * Writes to `BlackboardSlot::GBufferRenderTarget`.
 */
class GBufferPass final : public IFrameGraphPass {
public:
    GBufferPass(FrameGraphBlackboard* blackboard,
                rhi::IDevice* device,
                rhi::IPipeline* pipeline,
                const IMeshHandleResolver* meshResolver,
                const DrawList* drawList) noexcept;

    ~GBufferPass();

    const char* DebugName() const noexcept override { return "GBufferPass"; }

    /** Update frame-varying pointers so this persistent pass can be reused each frame without reallocation. */
    void Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline,
                     const IMeshHandleResolver* meshResolver, const DrawList* drawList) noexcept;

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    void Execute(rhi::ICommandList* cmd) override;

    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const override;

private:
    FrameGraphBlackboard* m_blackboard = nullptr;
    rhi::IDevice* m_device = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    const DrawList* m_drawList = nullptr;

    rhi::IDescriptorSet* m_frameSets[3] = {nullptr, nullptr, nullptr};
    uint64_t m_frameSetBuffersKeys[3] = {0, 0, 0};

    rhi::IDescriptorSet* m_passSets[3] = {nullptr, nullptr, nullptr};
    uint64_t m_passSetBuffersKeys[3] = {0, 0, 0};
};

} // namespace rendering
} // namespace eyelib
