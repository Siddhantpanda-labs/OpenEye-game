#include "eyelib/rendering/framegraph/passes/GBufferPass.h"

#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/rhi/RHI.h"


namespace eyelib {
namespace rendering {

GBufferPass::GBufferPass(FrameGraphBlackboard* blackboard, rhi::IDevice* device,
                         rhi::IPipeline* pipeline, const IMeshHandleResolver* meshResolver,
                         const DrawList* drawList) noexcept
    : m_blackboard(blackboard), m_device(device), m_pipeline(pipeline),
      m_meshResolver(meshResolver), m_drawList(drawList) {}

GBufferPass::~GBufferPass() {
    if (!m_device) return;
    for (int i = 0; i < 3; ++i) {
        if (m_frameSets[i]) m_device->DestroyDescriptorSet(m_frameSets[i]);
        if (m_passSets[i]) m_device->DestroyDescriptorSet(m_passSets[i]);
    }
}

void GBufferPass::Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline,
                               const IMeshHandleResolver* meshResolver, const DrawList* drawList) noexcept {
    m_blackboard = blackboard;
    m_pipeline = pipeline;
    m_meshResolver = meshResolver;
    m_drawList = drawList;
}

void GBufferPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(BlackboardSlot::GBufferRenderTarget);
    if (!rt) return;
    for (uint32_t i = 0; i < 3; ++i) {
        if (rhi::ITexture* color = rt->GetColorTexture(i))
            out.Push(color, rhi::ResourceState::RenderTarget);
    }
    if (rhi::ITexture* depth = rt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
}

bool GBufferPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const {
    outRt = blackboard.TryGetRenderTarget(BlackboardSlot::GBufferRenderTarget);
    if (!outRt) return false;

    outOps.color.loadOp = AttachmentLoadOp::Clear;
    outOps.color.storeOp = AttachmentStoreOp::Store;
    outOps.color.clearValue.rgba = {0.0f, 0.0f, 0.0f, 1.0f};
    
    outOps.hasDepth = outRt->GetDepthTexture() != nullptr;
    outOps.depth.loadOp = AttachmentLoadOp::Clear;
    outOps.depth.storeOp = AttachmentStoreOp::Store;
    outOps.depth.clearValue.depth = 1.0f;
    outOps.depth.clearValue.stencil = 0;
    
    return true;
}

void GBufferPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device || !m_meshResolver || !m_drawList)
        return;

    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(BlackboardSlot::GBufferRenderTarget);
    if (!rt) return;

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    rhi::IBuffer* perObject = m_blackboard->TryGetBuffer(BlackboardSlot::PerObjectStorageBuffer);
    rhi::IBuffer* dirLight = m_blackboard->TryGetBuffer(BlackboardSlot::DirectionalLightUniformBuffer);

    if (!perFrame || !perView || !perObject || !dirLight) return;

    const uint32_t frameIdx = m_device->GetCurrentFrameIndex();

    // Frame set
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

    // Pass set (reuses RasterForwardData layout: binding0=PerObject SSBO, binding1=DirLight UBO).
    // GBuffer shader only consumes binding0, but the layout contract requires both bindings to be written.
    const uint64_t dataKey = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perObject));
    if (!m_passSets[frameIdx] || dataKey != m_passSetBuffersKeys[frameIdx]) {
        if (!m_passSets[frameIdx]) {
            rhi::IPipelineLayout* dataLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::RasterForwardData);
            if (dataLayout) m_passSets[frameIdx] = m_device->CreateDescriptorSet(dataLayout);
        }
        m_passSetBuffersKeys[frameIdx] = dataKey;
        if (m_passSets[frameIdx]) {
            m_passSets[frameIdx]->UpdateStorageBuffer(0, perObject, 0, 0);
            m_passSets[frameIdx]->UpdateUniformBuffer(1, dirLight, 0, kDirectionalLightUniformGpuBytes);
        }
    }

    const uint32_t n = m_drawList->VisibleCount();
    if (n == 0 || !m_frameSets[frameIdx] || !m_passSets[frameIdx]) return;

    RenderAttachmentOps ops{};
    rhi::IRenderTarget* dummyRt = nullptr;
    if (!DescribeRenderAttachmentOps(*m_blackboard, dummyRt, ops)) return;

    cmd->SetRenderTarget(rt, ops);
    const uint32_t vw = rt->GetWidth();
    const uint32_t vh = rt->GetHeight();
    if (vw > 0 && vh > 0) cmd->SetViewport(0, 0, vw, vh);

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(false);

    cmd->BindPipeline(m_pipeline);
    rhi::IDescriptorSet* sets[2] = {m_frameSets[frameIdx], m_passSets[frameIdx]};
    cmd->BindDescriptorSets(0, 2, sets);

    for (uint32_t i = 0; i < n; ++i) {
        const DrawItem& it = m_drawList->items[i];
        if (it.layer != DrawLayer::Opaque) continue; // Only draw opaque in GBuffer

        const MeshHandle h = m_meshResolver->TryResolveMesh(it.meshHandle);
        if (!h.IsValid())
            continue;

        cmd->SetPushConstants(0, sizeof(uint32_t), &i);
        cmd->BindVertexBuffer(h.vertexBuffer, 0, 0);
        cmd->BindIndexBuffer(h.indexBuffer, h.indexType, 0);

        if (!h.submeshes.empty()) {
            for (const Submesh& sm : h.submeshes) {
                cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
            }
        } else {
            cmd->DrawIndexed(h.totalIndexCount, 0, 0);
        }
    }
}

} // namespace rendering
} // namespace eyelib
