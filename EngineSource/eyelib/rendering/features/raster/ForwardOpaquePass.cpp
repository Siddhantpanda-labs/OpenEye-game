#include "eyelib/rendering/features/raster/ForwardOpaquePass.h"

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
    if (!m_device) return;
    for (int i = 0; i < 3; ++i) {
        if (m_frameSets[i]) m_device->DestroyDescriptorSet(m_frameSets[i]);
        if (m_forwardDataSets[i]) m_device->DestroyDescriptorSet(m_forwardDataSets[i]);
    }
}

void ForwardOpaquePass::Reconfigure(FrameGraphBlackboard* blackboard, BlackboardSlot hdrSlot,
                                     rhi::IPipeline* pipeline, const IMeshHandleResolver* meshResolver,
                                     const DrawList* drawList) noexcept {
    m_blackboard = blackboard;
    m_hdrSlot = hdrSlot;
    m_pipeline = pipeline;
    m_meshResolver = meshResolver;
    m_drawList = drawList;
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

bool ForwardOpaquePass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                                    RenderAttachmentOps& outOps) const {
    outRt = blackboard.TryGetRenderTarget(m_hdrSlot);
    if (!outRt)
        return false;
    outOps.color.loadOp = AttachmentLoadOp::Clear;
    outOps.color.storeOp = AttachmentStoreOp::Store;
    outOps.color.clearValue.rgba = {0.12f, 0.14f, 0.20f, 1.0f};
    outOps.hasDepth = outRt->GetDepthTexture() != nullptr;
    outOps.depth.loadOp = AttachmentLoadOp::Clear;
    outOps.depth.storeOp = AttachmentStoreOp::Store;
    outOps.depth.clearValue.depth = 1.0f;
    outOps.depth.clearValue.stencil = 0u;
    return true;
}

void ForwardOpaquePass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device || !m_meshResolver || !m_drawList)
        return;

    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(m_hdrSlot);
    if (!rt)
        return;

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    rhi::IBuffer* perObject = m_blackboard->TryGetBuffer(BlackboardSlot::PerObjectStorageBuffer);
    rhi::IBuffer* dirLight = m_blackboard->TryGetBuffer(BlackboardSlot::DirectionalLightUniformBuffer);
    if (!perFrame || !perView || !perObject || !dirLight)
        return;

    const uint32_t frameIdx = m_device->GetCurrentFrameIndex();

    const uint64_t frameKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perFrame)) << 32) ^
                              static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perView));
    if (!m_frameSets[frameIdx] || frameKey != m_frameSetBuffersKeys[frameIdx]) {
        if (!m_frameSets[frameIdx]) {
            rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
            if (frameLayout) m_frameSets[frameIdx] = m_device->CreateDescriptorSet(frameLayout);
        }
        m_frameSetBuffersKeys[frameIdx] = frameKey;
        if (m_frameSets[frameIdx]) {
            m_frameSets[frameIdx]->UpdateUniformBuffer(0, perFrame, 0, kPerFrameUniformGpuBytes);
            m_frameSets[frameIdx]->UpdateUniformBuffer(1, perView, 0, kPerViewUniformGpuBytes);
        }
    }

    const uint64_t dataKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perObject)) << 32) ^
                             static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dirLight));
    if (!m_forwardDataSets[frameIdx] || dataKey != m_forwardDataBuffersKeys[frameIdx]) {
        if (!m_forwardDataSets[frameIdx]) {
            rhi::IPipelineLayout* dataLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::RasterForwardData);
            if (dataLayout) m_forwardDataSets[frameIdx] = m_device->CreateDescriptorSet(dataLayout);
        }
        m_forwardDataBuffersKeys[frameIdx] = dataKey;
        if (m_forwardDataSets[frameIdx]) {
            m_forwardDataSets[frameIdx]->UpdateStorageBuffer(0, perObject, 0, 0);
            m_forwardDataSets[frameIdx]->UpdateUniformBuffer(1, dirLight, 0, kDirectionalLightUniformGpuBytes);
        }
    }

    const uint32_t n = m_drawList->VisibleCount();
    if (n == 0 || !m_frameSets[frameIdx] || !m_forwardDataSets[frameIdx])
        return;

    RenderAttachmentOps attachmentOps{};
    rhi::IRenderTarget* declaredRt = nullptr;
    if (DescribeRenderAttachmentOps(*m_blackboard, declaredRt, attachmentOps) && declaredRt == rt)
        cmd->SetRenderTarget(rt, attachmentOps);
    else
        cmd->SetRenderTarget(rt);

    const uint32_t vw = rt->GetWidth();
    const uint32_t vh = rt->GetHeight();
    if (vw > 0 && vh > 0)
        cmd->SetViewport(0, 0, vw, vh);

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(false);

    cmd->BindPipeline(m_pipeline);
    rhi::IDescriptorSet* sets[2] = {m_frameSets[frameIdx], m_forwardDataSets[frameIdx]};
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
