#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h"

#include "eyelib/math/Frustum.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scene/DrawList.h" // DrawLayer enum

namespace eyelib {
namespace rendering {

namespace {
struct ShadowPushConstants {
    math::Mat4 lightViewProj{1.0f};
    math::Mat4 model{1.0f};
};
} // namespace

DirectionalShadowPass::DirectionalShadowPass(rhi::IRenderTarget* directionalShadowRt,
                                             rhi::IPipeline* shadowPipeline,
                                             const IMeshHandleResolver* meshResolver,
                                             const ShadowCasterList* casterList,
                                             const std::array<math::Mat4, 3>* lightViewProj) noexcept
    : m_directionalShadowRt(directionalShadowRt),
      m_shadowPipeline(shadowPipeline),
      m_meshResolver(meshResolver),
      m_casterList(casterList),
      m_lightViewProj(lightViewProj) {}

void DirectionalShadowPass::Reconfigure(rhi::IRenderTarget* directionalShadowRt,
                                        rhi::IPipeline* shadowPipeline,
                                        const IMeshHandleResolver* meshResolver,
                                        const ShadowCasterList* casterList,
                                        const std::array<math::Mat4, 3>* lightViewProj) noexcept {
    m_directionalShadowRt = directionalShadowRt;
    m_shadowPipeline = shadowPipeline;
    m_meshResolver = meshResolver;
    m_casterList = casterList;
    m_lightViewProj = lightViewProj;
}

void DirectionalShadowPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    (void)blackboard;
    if (!m_directionalShadowRt)
        return;
    if (rhi::ITexture* depth = m_directionalShadowRt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
    if (rhi::ITexture* color = m_directionalShadowRt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
}

bool DirectionalShadowPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const {
    (void)blackboard;
    outRt = m_directionalShadowRt;
    if (!outRt)
        return false;
    outOps.hasDepth = outRt->GetDepthTexture() != nullptr;
    outOps.depth.loadOp = AttachmentLoadOp::Clear;
    outOps.depth.storeOp = AttachmentStoreOp::Store;
    outOps.depth.clearValue.depth = 1.0f;
    outOps.depth.clearValue.stencil = 0;
    if (outRt->GetColorTexture(0) != nullptr) {
        outOps.color.loadOp = AttachmentLoadOp::Clear;
        outOps.color.storeOp = AttachmentStoreOp::Store;
        outOps.color.clearValue.rgba = {1.0f, 1.0f, 1.0f, 1.0f};
    }
    return true;
}

void DirectionalShadowPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_directionalShadowRt || !m_shadowPipeline || !m_meshResolver || !m_casterList || !m_lightViewProj)
        return;
    RenderAttachmentOps ops{};
    ops.hasDepth = m_directionalShadowRt->GetDepthTexture() != nullptr;
    ops.depth.loadOp = AttachmentLoadOp::Clear;
    ops.depth.storeOp = AttachmentStoreOp::Store;
    ops.depth.clearValue.depth = 1.0f;
    ops.depth.clearValue.stencil = 0;
    if (m_directionalShadowRt->GetColorTexture(0) != nullptr) {
        ops.color.loadOp = AttachmentLoadOp::Clear;
        ops.color.storeOp = AttachmentStoreOp::Store;
        ops.color.clearValue.rgba = {1.0f, 1.0f, 1.0f, 1.0f};
    }
    if (!ops.hasDepth && m_directionalShadowRt->GetColorTexture(0) == nullptr)
        return;
    const uint32_t w = m_directionalShadowRt->GetWidth();
    const uint32_t h = m_directionalShadowRt->GetHeight();

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(true);
    // Directional shadows cover large world extents; a stronger slope-scaled caster bias
    // is required to suppress lit-side acne on high-frequency geometry/material regions.
    cmd->SetDepthBias(1.2f, 3.0f);
    cmd->BindPipeline(m_shadowPipeline);

    for (uint32_t layer = 0; layer < 3u; ++layer) {
        m_directionalShadowRt->SetDepthLayer(layer);
        cmd->SetRenderTarget(m_directionalShadowRt, ops);
        if (w > 0 && h > 0)
            cmd->SetViewport(0, 0, w, h);

        math::Frustum lightFrustum;
        lightFrustum.FromMatrix((*m_lightViewProj)[layer]);

        const uint32_t n = m_casterList->Count();
        for (uint32_t i = 0; i < n; ++i) {
            const ShadowCasterItem& it = m_casterList->items[i];
            if (it.layer != DrawLayer::Opaque && it.layer != DrawLayer::AlphaTest)
                continue;
            if (!it.worldBounds.IsValid())
                continue;
            if (lightFrustum.Contains(it.worldBounds) == 0)
                continue;

            const MeshHandle hMesh = m_meshResolver->TryResolveMesh(it.meshHandle);
            if (!hMesh.IsValid())
                continue;

            cmd->BindVertexBuffer(hMesh.vertexBuffer, 0, 0);
            cmd->BindIndexBuffer(hMesh.indexBuffer, hMesh.indexType, 0);

            ShadowPushConstants pc{};
            pc.lightViewProj = (*m_lightViewProj)[layer];
            pc.model = it.worldMatrix;
            cmd->SetPushConstants(0, sizeof(ShadowPushConstants), &pc);

            if (!hMesh.submeshes.empty()) {
                for (const Submesh& sm : hMesh.submeshes)
                    cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
            } else {
                cmd->DrawIndexed(hMesh.totalIndexCount, 0, 0);
            }
        }
    }
}

}  // namespace rendering
}  // namespace eyelib
