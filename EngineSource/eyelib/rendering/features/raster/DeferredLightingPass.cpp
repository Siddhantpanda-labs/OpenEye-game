#include "eyelib/rendering/features/raster/DeferredLightingPass.h"

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {

DeferredLightingPass::DeferredLightingPass(FrameGraphBlackboard* blackboard, rhi::IDevice* device,
                                           rhi::IPipeline* pipeline) noexcept
    : m_blackboard(blackboard), m_device(device), m_pipeline(pipeline) {}

DeferredLightingPass::~DeferredLightingPass() {
    if (!m_device) return;
    for (int i = 0; i < 3; ++i) {
        if (m_frameSets[i]) m_device->DestroyDescriptorSet(m_frameSets[i]);
        if (m_passSets[i]) m_device->DestroyDescriptorSet(m_passSets[i]);
    }
}

void DeferredLightingPass::Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline) noexcept {
    m_blackboard = blackboard;
    m_pipeline = pipeline;
}

void DeferredLightingPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* hdr = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    rhi::IRenderTarget* gbuffer = blackboard.TryGetRenderTarget(BlackboardSlot::GBufferRenderTarget);

    if (hdr) {
        if (rhi::ITexture* color = hdr->GetColorTexture(0))
            out.Push(color, rhi::ResourceState::RenderTarget);
    }

    if (gbuffer) {
        for (uint32_t i = 0; i < 3; ++i) {
            if (rhi::ITexture* color = gbuffer->GetColorTexture(i))
                out.Push(color, rhi::ResourceState::ShaderResource);
        }
        // Depth must be the G-buffer depth (written by GBufferPass), not Main HDR depth (unused in deferred path).
        if (rhi::ITexture* depth = gbuffer->GetDepthTexture())
            out.Push(depth, rhi::ResourceState::DepthRead);
    }

    rhi::ITexture* iblIrr = blackboard.TryGetTexture(BlackboardSlot::IblIrradianceCubemap);
    rhi::ITexture* iblPre = blackboard.TryGetTexture(BlackboardSlot::IblPrefilterCubemap);
    if (iblIrr)
        out.Push(iblIrr, rhi::ResourceState::ShaderResource);
    if (iblPre && iblPre != iblIrr)
        out.Push(iblPre, rhi::ResourceState::ShaderResource);
    if (rhi::ITexture* iblLut = blackboard.TryGetTexture(BlackboardSlot::IblBrdfLut))
        out.Push(iblLut, rhi::ResourceState::ShaderResource);
    if (rhi::ITexture* dirShadow = blackboard.TryGetTexture(BlackboardSlot::DirectionalShadowMapArray))
        out.Push(dirShadow, rhi::ResourceState::DepthRead);
}

bool DeferredLightingPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const {
    outRt = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!outRt) return false;

    outOps.color.loadOp = AttachmentLoadOp::Clear;
    outOps.color.storeOp = AttachmentStoreOp::Store;
    outOps.color.clearValue.rgba = {0.1f, 0.15f, 0.2f, 1.0f};
    
    outOps.hasDepth = false; // Deferred lighting is a fullscreen pass; depth is read as a sampler, not bound as an attachment.

    
    return true;
}

void DeferredLightingPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device)
        return;

    rhi::IRenderTarget* hdr = m_blackboard->TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    rhi::IRenderTarget* gbuffer = m_blackboard->TryGetRenderTarget(BlackboardSlot::GBufferRenderTarget);
    if (!hdr || !gbuffer)
        return;

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    rhi::IBuffer* dirLight = m_blackboard->TryGetBuffer(BlackboardSlot::DirectionalLightUniformBuffer);
    rhi::IBuffer* shadowParams = m_blackboard->TryGetBuffer(BlackboardSlot::ShadowParamsBuffer);

    if (!perFrame || !perView || !dirLight || !shadowParams)
        return;

    const uint32_t frameIdx = m_device->GetCurrentFrameIndex();

    // Frame set
    const uint64_t frameKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perFrame)) << 32) ^
                              static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perView));
    if (!m_frameSets[frameIdx] || frameKey != m_frameSetKeys[frameIdx]) {
        if (!m_frameSets[frameIdx]) {
            rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
            if (frameLayout) m_frameSets[frameIdx] = m_device->CreateDescriptorSet(frameLayout);
        }
        m_frameSetKeys[frameIdx] = frameKey;
        if (m_frameSets[frameIdx]) {
            m_frameSets[frameIdx]->UpdateUniformBuffer(0, perFrame, 0, kPerFrameUniformGpuBytes);
            m_frameSets[frameIdx]->UpdateUniformBuffer(1, perView, 0, kPerViewUniformGpuBytes);
        }
    }

    // Pass set: binding 0..2 GBuffer MRTs, binding 3 = G-buffer depth (same surface GBufferPass wrote).
    // Cache key must include **every** bound texture identity. G-buffer color images are recreated on resize /
    // `EnsureAllocated` while `MainDepth` can stay the same pointer — omitting color pointers leaves stale
    // COMBINED_IMAGE_SAMPLER descriptors pointing at freed images (flat / undefined sampling).
    rhi::ITexture* depthTex = gbuffer->GetDepthTexture();
    rhi::ITexture* c0 = gbuffer->GetColorTexture(0);
    rhi::ITexture* c1 = gbuffer->GetColorTexture(1);
    rhi::ITexture* c2 = gbuffer->GetColorTexture(2);
    rhi::ITexture* iblIrr = m_blackboard->TryGetTexture(BlackboardSlot::IblIrradianceCubemap);
    rhi::ITexture* iblSpec = m_blackboard->TryGetTexture(BlackboardSlot::IblPrefilterCubemap);
    rhi::ITexture* iblLut = m_blackboard->TryGetTexture(BlackboardSlot::IblBrdfLut);
    rhi::ITexture* dirShadow = m_blackboard->TryGetTexture(BlackboardSlot::DirectionalShadowMapArray);
    const uint64_t dataKey = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dirLight)) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c0)) << 3u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c1)) << 7u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c2)) << 11u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(depthTex)) << 17u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(iblIrr)) << 21u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(iblSpec)) << 25u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(iblLut)) << 29u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shadowParams)) << 33u) ^
                             (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dirShadow)) << 37u);
    if (!m_passSets[frameIdx] || dataKey != m_passSetKeys[frameIdx]) {
        if (!m_passSets[frameIdx]) {
            rhi::IPipelineLayout* passLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::RasterDeferredLightingData);
            if (passLayout) m_passSets[frameIdx] = m_device->CreateDescriptorSet(passLayout);
        }
        m_passSetKeys[frameIdx] = dataKey;
        if (m_passSets[frameIdx]) {
            m_passSets[frameIdx]->UpdateTexture(0, gbuffer->GetColorTexture(0));
            m_passSets[frameIdx]->UpdateTexture(1, gbuffer->GetColorTexture(1));
            m_passSets[frameIdx]->UpdateTexture(2, gbuffer->GetColorTexture(2));
            m_passSets[frameIdx]->UpdateTexture(3, depthTex); // Read depth (now uses DEPTH_STENCIL_READ_ONLY_OPTIMAL)
            m_passSets[frameIdx]->UpdateUniformBuffer(4, dirLight, 0, kDirectionalLightUniformGpuBytes);
            // IBL: blackboard from `RenderResources`; `UpdateTexture(nullptr)` uses engine fallbacks when absent.
            m_passSets[frameIdx]->UpdateTexture(5, iblIrr);
            m_passSets[frameIdx]->UpdateTexture(6, iblSpec);
            m_passSets[frameIdx]->UpdateTexture(7, iblLut);
            m_passSets[frameIdx]->UpdateUniformBuffer(8, shadowParams, 0, 0);
            m_passSets[frameIdx]->UpdateTexture(9, dirShadow);
        }
    }

    if (!m_frameSets[frameIdx] || !m_passSets[frameIdx])
        return;

    RenderAttachmentOps ops{};
    rhi::IRenderTarget* dummyRt = nullptr;
    if (!DescribeRenderAttachmentOps(*m_blackboard, dummyRt, ops))
        return;

    cmd->SetRenderTarget(hdr, ops);
    const uint32_t vw = hdr->GetWidth();
    const uint32_t vh = hdr->GetHeight();
    if (vw > 0 && vh > 0) cmd->SetViewport(0, 0, vw, vh);

    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);

    cmd->BindPipeline(m_pipeline);
    rhi::IDescriptorSet* sets[2] = {m_frameSets[frameIdx], m_passSets[frameIdx]};
    cmd->BindDescriptorSets(0, 2, sets);

    // Draw fullscreen triangle
    cmd->Draw(3, 0);
}

} // namespace rendering
} // namespace eyelib
