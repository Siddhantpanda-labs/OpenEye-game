#include "eyelib/rendering/framegraph/passes/FullscreenHdrComposePass.h"

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

FullscreenHdrComposePass::FullscreenHdrComposePass(FrameGraphBlackboard* blackboard, BlackboardSlot slot, rhi::IDevice* device,
                                                 rhi::IPipeline* pipeline) noexcept
    : m_blackboard(blackboard)
    , m_slot(slot)
    , m_device(device)
    , m_pipeline(pipeline) {}

FullscreenHdrComposePass::~FullscreenHdrComposePass() {
    if (m_device) {
        for (int i = 0; i < 3; ++i) {
            if (m_frameSets[i]) m_device->DestroyDescriptorSet(m_frameSets[i]);
        }
    }
}

void FullscreenHdrComposePass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device)
        return;
    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_slot);
    if (!rt)
        return;

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    if (perFrame && perView) {
        const uint32_t frameIdx = m_device->GetCurrentFrameIndex();
        const uint64_t key = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perFrame)) << 32) ^
                             static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perView));
        if (!m_frameSets[frameIdx] || key != m_frameSetBuffersKeys[frameIdx]) {
            if (!m_frameSets[frameIdx]) {
                rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
                if (frameLayout) m_frameSets[frameIdx] = m_device->CreateDescriptorSet(frameLayout);
            }
            m_frameSetBuffersKeys[frameIdx] = key;
        }
        if (m_frameSets[frameIdx]) {
            m_frameSets[frameIdx]->UpdateUniformBuffer(0, perFrame, 0, kPerFrameUniformGpuBytes);
            m_frameSets[frameIdx]->UpdateUniformBuffer(1, perView, 0, kPerViewUniformGpuBytes);
        }

        const float tint[4] = {0.18f, 0.42f, 0.62f, 1.0f};
        cmd->SetRenderTarget(rt);
        cmd->SetPushConstants(0, sizeof(tint), tint);
        cmd->BindPipeline(m_pipeline);
        if (m_frameSets[frameIdx])
            cmd->BindDescriptorSets(0, 1, &m_frameSets[frameIdx]);
        cmd->DrawFullscreenQuad(m_pipeline);
    }
}

}  // namespace rendering
}  // namespace eyelib
