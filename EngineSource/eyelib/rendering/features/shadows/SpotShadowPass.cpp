#include "eyelib/rendering/features/shadows/SpotShadowPass.h"

#include "eyelib/math/Frustum.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scene/DrawList.h"

namespace eyelib {
namespace rendering {

namespace {
struct SpotShadowPushConstants {
    math::Mat4 lightViewProj{1.0f};
    math::Mat4 model{1.0f};
};
} // namespace

void SpotShadowPass::Setup(rhi::IRenderTarget* spotShadowRt,
                           rhi::IPipeline* shadowPipeline,
                           const IMeshHandleResolver* meshResolver,
                           const ShadowCasterList* casterList,
                           const std::vector<math::Mat4>& spotLightViewProjs) noexcept {
    m_spotShadowRt = spotShadowRt;
    m_shadowPipeline = shadowPipeline;
    m_meshResolver = meshResolver;
    m_casterList = casterList;
    m_spotLightViewProjs = spotLightViewProjs;
}

void SpotShadowPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    (void)blackboard;
    if (!m_spotShadowRt) return;
    if (rhi::ITexture* depth = m_spotShadowRt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
    if (rhi::ITexture* color = m_spotShadowRt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
}

bool SpotShadowPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const {
    (void)blackboard;
    outRt = m_spotShadowRt;
    if (!outRt) return false;
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

void SpotShadowPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_spotShadowRt || !m_shadowPipeline || !m_meshResolver || !m_casterList || m_spotLightViewProjs.empty())
        return;

    RenderAttachmentOps ops{};
    ops.hasDepth = m_spotShadowRt->GetDepthTexture() != nullptr;
    ops.depth.loadOp = AttachmentLoadOp::Clear;
    ops.depth.storeOp = AttachmentStoreOp::Store;
    ops.depth.clearValue.depth = 1.0f;
    ops.depth.clearValue.stencil = 0;
    if (m_spotShadowRt->GetColorTexture(0) != nullptr) {
        ops.color.loadOp = AttachmentLoadOp::Clear;
        ops.color.storeOp = AttachmentStoreOp::Store;
        ops.color.clearValue.rgba = {1.0f, 1.0f, 1.0f, 1.0f};
    }
    if (!ops.hasDepth && m_spotShadowRt->GetColorTexture(0) == nullptr)
        return;

    const uint32_t w = m_spotShadowRt->GetWidth();
    const uint32_t h = m_spotShadowRt->GetHeight();

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(true);
    cmd->SetDepthBias(1.0f, 2.0f);
    cmd->BindPipeline(m_shadowPipeline);

    const uint32_t layerCount = static_cast<uint32_t>(m_spotLightViewProjs.size());
    for (uint32_t layer = 0; layer < layerCount; ++layer) {
        m_spotShadowRt->SetDepthLayer(layer);
        cmd->SetRenderTarget(m_spotShadowRt, ops);
        if (w > 0 && h > 0)
            cmd->SetViewport(0, 0, w, h);

        // Build frustum for spot light culling
        math::Frustum lightFrustum;
        lightFrustum.FromMatrix(m_spotLightViewProjs[layer]);

        const uint32_t n = m_casterList->Count();
        for (uint32_t i = 0; i < n; ++i) {
            const ShadowCasterItem& it = m_casterList->items[i];
            if (it.layer != DrawLayer::Opaque && it.layer != DrawLayer::AlphaTest)
                continue;
            if (!it.worldBounds.IsValid())
                continue;
            // Cull objects outside this spot light's frustum
            if (lightFrustum.Contains(it.worldBounds) == 0)
                continue;

            const MeshHandle hMesh = m_meshResolver->TryResolveMesh(it.meshHandle);
            if (!hMesh.IsValid())
                continue;

            cmd->BindVertexBuffer(hMesh.vertexBuffer, 0, 0);
            cmd->BindIndexBuffer(hMesh.indexBuffer, hMesh.indexType, 0);

            SpotShadowPushConstants pc{};
            pc.lightViewProj = m_spotLightViewProjs[layer];
            pc.model = it.worldMatrix;  // ShadowCasterItem::worldMatrix
            cmd->SetPushConstants(0, sizeof(SpotShadowPushConstants), &pc);

            if (!hMesh.submeshes.empty()) {
                for (const Submesh& sm : hMesh.submeshes)
                    cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
            } else {
                cmd->DrawIndexed(hMesh.totalIndexCount, 0, 0);
            }
        }

        // After first layer, switch to Load so we don't clear subsequent layers
        ops.depth.loadOp = AttachmentLoadOp::Load;
        ops.color.loadOp = AttachmentLoadOp::Load;
    }
}

}  // namespace rendering
}  // namespace eyelib
