#include "eyelib/rendering/features/raster/SkyboxPass.h"

#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <cstring>

namespace eyelib {
namespace rendering {

namespace {

/** Push-constant layout shared by both shader variants. 144 bytes; the runtime push range
 *  is 256 bytes so this is comfortably inside the budget (see VulkanResourceSpaces.cpp
 *  `kPushConstantSize`). The cubemap variant samples the brightness directly; the
 *  procedural variant treats it as a global gain on the gradient. */
struct SkyboxPushConstants {
    math::Mat4 inverseView;       // offset   0 (64 bytes)
    math::Mat4 inverseProjection; // offset  64 (64 bytes)
    float      brightness;        // offset 128 (4 bytes)
    float      pad0;
    float      pad1;
    float      pad2;
};

}  // namespace

SkyboxPass::SkyboxPass(FrameGraphBlackboard* blackboard, rhi::IDevice* device,
                       rhi::IPipeline* cubemapPipeline,
                       rhi::IPipeline* proceduralPipeline) noexcept
    : m_blackboard(blackboard),
      m_device(device),
      m_cubemapPipeline(cubemapPipeline),
      m_proceduralPipeline(proceduralPipeline) {}

SkyboxPass::~SkyboxPass() {
    DestroyDeviceResources();
}

void SkyboxPass::Reconfigure(FrameGraphBlackboard* blackboard, rhi::IDevice* device,
                             rhi::IPipeline* cubemapPipeline,
                             rhi::IPipeline* proceduralPipeline) noexcept {
    m_blackboard = blackboard;
    m_device = device;
    m_cubemapPipeline = cubemapPipeline;
    m_proceduralPipeline = proceduralPipeline;
}

void SkyboxPass::DestroyDeviceResources() noexcept {
    if (!m_device) return;
    for (uint32_t i = 0; i < kFrameSlots; ++i) {
        if (m_cubemapSets[i]) {
            m_device->DestroyDescriptorSet(m_cubemapSets[i]);
            m_cubemapSets[i] = nullptr;
        }
        m_cubemapSetTextures[i] = nullptr;
    }
}

void SkyboxPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* hdr = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!hdr) return;
    if (rhi::ITexture* color = hdr->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
    // Sample the existing depth as DepthRead so opaque geometry occludes the sky correctly
    // without disturbing the depth buffer for any subsequent overlay passes.
    if (rhi::ITexture* depth = hdr->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthRead);
    if (m_request.kind == SkyboxRequest::Kind::Cubemap && m_request.cubemap)
        out.Push(m_request.cubemap, rhi::ResourceState::ShaderResource);
}

bool SkyboxPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                             RenderAttachmentOps& outOps) const {
    outRt = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!outRt) return false;

    // Overlay-style: HDR colour from the lighting pass is preserved, depth is read-only.
    outOps.color.loadOp  = AttachmentLoadOp::Load;
    outOps.color.storeOp = AttachmentStoreOp::Store;
    outOps.hasDepth      = outRt->GetDepthTexture() != nullptr;
    outOps.depth.loadOp  = AttachmentLoadOp::Load;
    outOps.depth.storeOp = AttachmentStoreOp::Store;
    return true;
}

void SkyboxPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_device || !m_blackboard || m_request.kind == SkyboxRequest::Kind::None) return;

    rhi::IPipeline* pipeline = nullptr;
    if (m_request.kind == SkyboxRequest::Kind::Cubemap) {
        if (!m_request.cubemap || !m_request.cubemap->IsGpuBindable()) return;
        pipeline = m_cubemapPipeline;
    } else {
        pipeline = m_proceduralPipeline;
    }
    if (!pipeline) return;

    // The frame graph executor closes dynamic rendering after every pass (see FrameGraph::Execute),
    // so each pass is responsible for re-binding its render target. Mirrors GBufferPass /
    // DeferredLightingPass / DebugDrawPass.
    RenderAttachmentOps ops{};
    rhi::IRenderTarget* rt = nullptr;
    if (!DescribeRenderAttachmentOps(*m_blackboard, rt, ops) || !rt) return;
    cmd->SetRenderTarget(rt, ops);
    const uint32_t vw = rt->GetWidth();
    const uint32_t vh = rt->GetHeight();
    if (vw > 0 && vh > 0) cmd->SetViewport(0, 0, vw, vh);

    cmd->SetCullFace(false);
    // Depth state matches the shader's intent: test-only, LessEqual via the pipeline's
    // depthStencilOverride (see SkyboxFeature::MakeCubemapSpec / MakeProceduralSpec).
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(false);
    cmd->SetDepthFuncLess(false);

    cmd->BindPipeline(pipeline);

    if (m_request.kind == SkyboxRequest::Kind::Cubemap) {
        const uint32_t frameIdx = m_device->GetCurrentFrameIndex() % kFrameSlots;
        if (!m_cubemapSets[frameIdx]) {
            rhi::IPipelineLayout* iblLayout =
                m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::IBLPass);
            if (iblLayout) m_cubemapSets[frameIdx] = m_device->CreateDescriptorSet(iblLayout);
        }
        if (!m_cubemapSets[frameIdx]) return;

        // Re-bind only when the source texture pointer changes (typical: identity stable
        // across frames; cache invalidation only when the editor sky panel hot-swaps the asset).
        if (m_cubemapSetTextures[frameIdx] != m_request.cubemap) {
            m_cubemapSets[frameIdx]->UpdateTexture(0, m_request.cubemap);
            m_cubemapSetTextures[frameIdx] = m_request.cubemap;
        }
        cmd->BindDescriptorSets(0, 1, &m_cubemapSets[frameIdx]);
    }
    // Procedural variant: PushOnly layout, no descriptor sets to bind.

    SkyboxPushConstants pc{};
    pc.inverseView       = m_inverseView;
    pc.inverseProjection = m_inverseProjection;
    pc.brightness        = (m_request.brightness < 0.0f) ? 0.0f : m_request.brightness;
    pc.pad0 = pc.pad1 = pc.pad2 = 0.0f;
    cmd->SetPushConstants(0, sizeof(SkyboxPushConstants), &pc);

    // Full-screen triangle generated from gl_VertexIndex; no VBO bound.
    cmd->Draw(3, 0);
}

}  // namespace rendering
}  // namespace eyelib
