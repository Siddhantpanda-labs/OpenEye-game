#pragma once

#include "eyelib/rendering/features/editor/DebugLine.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {

class FrameGraphBlackboard;
struct FrameGraphBarrierList;

namespace rhi {
struct IBuffer;
struct ICommandList;
struct IDescriptorSet;
struct IDevice;
struct IPipeline;
struct IRenderTarget;
}

/**
 * Editor debug-line overlay pass.
 *
 * Reads world-space `DebugLine` vertices from `EditorFeature::SetLines`, uploads them into a
 * per-frame host-visible vertex buffer (triple-buffered for in-flight safety), then issues
 * a single `Draw` over the `MainHdrRenderTarget` using the existing `MainDepth` attachment
 * in depth-read-only mode (lines occlude correctly against opaque geometry; no z-write).
 *
 * Schedules only when the feature has lines for the current frame — empty-source frames carry
 * zero pass cost (the feature's `OnSchedulePasses` skips this pass entirely).
 */
class DebugDrawPass final : public IFrameGraphPass {
public:
    DebugDrawPass(FrameGraphBlackboard* blackboard, rhi::IDevice* device, rhi::IPipeline* pipeline) noexcept;
    ~DebugDrawPass() override;

    /** Reuse the persistent pass across frames; updates frame-varying pointers without realloc. */
    void Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline) noexcept;

    /** Borrowed pointer to a contiguous array of lines (owned by `EditorFeature`); valid until the next call. */
    void SetLines(const DebugLine* lines, size_t count) noexcept;

    const char* DebugName() const noexcept override { return pass_names::kEditorDebugLines; }

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                     RenderAttachmentOps& outOps) const override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    void DestroyDeviceResources() noexcept;
    void EnsureVertexBufferCapacity(uint32_t frameIdx, size_t requiredBytes);

    FrameGraphBlackboard* m_blackboard = nullptr;
    rhi::IDevice* m_device = nullptr;
    rhi::IPipeline* m_pipeline = nullptr;

    const DebugLine* m_lines = nullptr;
    size_t m_lineCount = 0;

    static constexpr uint32_t kFrameSlots = 3;
    rhi::IBuffer* m_vertexBuffers[kFrameSlots] = {nullptr, nullptr, nullptr};
    size_t m_vertexBufferCapacityBytes[kFrameSlots] = {0, 0, 0};

    rhi::IDescriptorSet* m_frameSets[kFrameSlots] = {nullptr, nullptr, nullptr};
    uint64_t m_frameSetBuffersKeys[kFrameSlots] = {0, 0, 0};
};

}  // namespace rendering
}  // namespace eyelib
