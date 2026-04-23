#include "eyelib/rendering/framegraph/passes/ForwardOpaquePass.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

ForwardOpaquePass::ForwardOpaquePass(FrameGraphBlackboard* blackboard, BlackboardSlot hdrSlot, rhi::IDevice* device,
                                     rhi::IPipeline* pipeline, const IMeshHandleResolver* meshResolver, const DrawList* drawList) noexcept
    : m_blackboard(blackboard)
    , m_hdrSlot(hdrSlot)
    , m_device(device)
    , m_pipeline(pipeline)
    , m_meshResolver(meshResolver)
    , m_drawList(drawList) {}

ForwardOpaquePass::~ForwardOpaquePass() {
    if (!m_device)
        return;
    if (m_frameSet) {
        m_device->DestroyDescriptorSet(m_frameSet);
        m_frameSet = nullptr;
    }
    if (m_forwardDataSet) {
        m_device->DestroyDescriptorSet(m_forwardDataSet);
        m_forwardDataSet = nullptr;
    }
}

void ForwardOpaquePass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(m_hdrSlot);
    if (!rt)
        return;
    if (rhi::ITexture* color = rt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
    if (rhi::ITexture* depth = rt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
}

void ForwardOpaquePass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device || !m_meshResolver || !m_drawList) {
        if (!m_diagLoggedEarlyExit) {
            m_diagLoggedEarlyExit = true;
            core::Log::WarnF("ForwardOpaquePass::Execute early-exit: cmd=%s blackboard=%s pipeline=%s device=%s meshResolver=%s drawList=%s",
                             cmd ? "ok" : "null", m_blackboard ? "ok" : "null", m_pipeline ? "ok" : "null",
                             m_device ? "ok" : "null", m_meshResolver ? "ok" : "null", m_drawList ? "ok" : "null");
        }
        return;
    }
    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_hdrSlot);
    if (!rt) {
        if (!m_diagLoggedNoRt) {
            m_diagLoggedNoRt = true;
            core::Log::Warn("ForwardOpaquePass::Execute: blackboard MainHdrRenderTarget is null.");
        }
        return;
    }

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    rhi::IBuffer* perObject = m_blackboard->TryGetBuffer(BlackboardSlot::PerObjectStorageBuffer);
    rhi::IBuffer* dirLight = m_blackboard->TryGetBuffer(BlackboardSlot::DirectionalLightUniformBuffer);
    if (!perFrame || !perView || !perObject || !dirLight) {
        if (!m_diagLoggedBuffersMissing) {
            m_diagLoggedBuffersMissing = true;
            core::Log::WarnF("ForwardOpaquePass::Execute: missing blackboard buffer(s) perFrame=%s perView=%s perObject=%s dirLight=%s",
                             perFrame ? "ok" : "null", perView ? "ok" : "null", perObject ? "ok" : "null", dirLight ? "ok" : "null");
        }
        return;
    }

    const uint64_t frameKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perFrame)) << 32) ^
                              static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perView));
    if (!m_frameSet || frameKey != m_frameSetBuffersKey) {
        if (m_frameSet) {
            m_device->DestroyDescriptorSet(m_frameSet);
            m_frameSet = nullptr;
        }
        rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
        if (frameLayout)
            m_frameSet = m_device->CreateDescriptorSet(frameLayout);
        m_frameSetBuffersKey = frameKey;
    }
    if (m_frameSet) {
        m_frameSet->UpdateUniformBuffer(0, perFrame, 0, kPerFrameUniformGpuBytes);
        m_frameSet->UpdateUniformBuffer(1, perView, 0, kPerViewUniformGpuBytes);
    }

    const uint64_t dataKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perObject)) << 32) ^
                             static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dirLight));
    if (!m_forwardDataSet || dataKey != m_forwardDataBuffersKey) {
        if (m_forwardDataSet) {
            m_device->DestroyDescriptorSet(m_forwardDataSet);
            m_forwardDataSet = nullptr;
        }
        rhi::IPipelineLayout* dataLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::RasterForwardM5Data);
        if (dataLayout)
            m_forwardDataSet = m_device->CreateDescriptorSet(dataLayout);
        m_forwardDataBuffersKey = dataKey;
    }
    if (m_forwardDataSet) {
        m_forwardDataSet->UpdateStorageBuffer(0, perObject, 0, 0);
        m_forwardDataSet->UpdateUniformBuffer(1, dirLight, 0, kDirectionalLightUniformGpuBytes);
    }

    const uint32_t n = m_drawList->VisibleCount();
    if (n == 0 || !m_frameSet || !m_forwardDataSet) {
        if (!m_diagLoggedNoDraws) {
            m_diagLoggedNoDraws = true;
            core::Log::WarnF("ForwardOpaquePass::Execute: nothing to draw (visible=%u frameSet=%s forwardDataSet=%s).",
                             n, m_frameSet ? "ok" : "null", m_forwardDataSet ? "ok" : "null");
        }
        return;
    }

    if (!m_diagLoggedFirstDraw) {
        m_diagLoggedFirstDraw = true;
        core::Log::InfoF("ForwardOpaquePass::Execute: first draw path reached (visible=%u).", n);
        core::Log::InfoF(
            "ForwardOpaquePass::Execute buffers/sets: perFrame=%p perView=%p perObject=%p dirLight=%p frameSet=%p forwardDataSet=%p",
            static_cast<void*>(perFrame),
            static_cast<void*>(perView),
            static_cast<void*>(perObject),
            static_cast<void*>(dirLight),
            static_cast<void*>(m_frameSet),
            static_cast<void*>(m_forwardDataSet));
    }

    cmd->SetRenderTarget(rt);
    const uint32_t vw = rt->GetWidth();
    const uint32_t vh = rt->GetHeight();
    if (vw > 0 && vh > 0)
        cmd->SetViewport(0, 0, vw, vh);

    // Pipeline requests depth LESS test with write enabled; ensure dynamic state matches the pipeline
    // (BindPipeline on Vulkan propagates the command list's tracked depth state via dynamic state commands,
    // so lingering values from earlier passes like the present-resolve blit would otherwise disable depth here).
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(false);

    cmd->BindPipeline(m_pipeline);
    rhi::IDescriptorSet* sets[2] = {m_frameSet, m_forwardDataSet};
    cmd->BindDescriptorSets(0, 2, sets);

    for (uint32_t i = 0; i < n; ++i) {
        const DrawItem& it = m_drawList->items[i];
        const MeshHandle h = m_meshResolver->TryResolveMesh(it.meshHandle);
        if (!h.IsValid())
            continue;
        cmd->SetPushConstants(0, sizeof(uint32_t), &i);
        cmd->BindVertexBuffer(h.vertexBuffer, 0, 0);
        cmd->BindIndexBuffer(h.indexBuffer, h.indexType, 0);
        if (!h.submeshes.empty()) {
            for (const Submesh& sm : h.submeshes)
                cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
        } else {
            cmd->DrawIndexed(h.totalIndexCount, 0, 0);
        }
    }
}

}  // namespace rendering
}  // namespace eyelib
