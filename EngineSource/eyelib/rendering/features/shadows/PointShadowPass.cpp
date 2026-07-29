#include "eyelib/rendering/features/shadows/PointShadowPass.h"

#include "eyelib/math/Frustum.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scene/DrawList.h"

namespace eyelib {
namespace rendering {

namespace {
struct ShadowPushConstants {
    math::Mat4 lightViewProj{1.0f};
    uint32_t baseObjectIndex = 0;
};
}  // namespace

void PointShadowPass::Setup(rhi::IRenderTarget* pointShadowRt,
                            rhi::IPipeline* shadowPipeline,
                            const IMeshHandleResolver* meshResolver,
                            const ShadowCasterList* casterList,
                            rhi::IDescriptorSet* shadowDataSet,
                            const std::vector<math::Mat4>& pointFaceViewProjs) noexcept {
    m_pointShadowRt = pointShadowRt;
    m_shadowPipeline = shadowPipeline;
    m_meshResolver = meshResolver;
    m_casterList = casterList;
    m_shadowDataSet = shadowDataSet;
    m_pointFaceViewProjs = pointFaceViewProjs;
}

void PointShadowPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    (void)blackboard;
    if (!m_pointShadowRt) return;
    if (rhi::ITexture* depth = m_pointShadowRt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthWrite);
    if (rhi::ITexture* color = m_pointShadowRt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
}

bool PointShadowPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                                  RenderAttachmentOps& outOps) const {
    (void)blackboard;
    outRt = m_pointShadowRt;
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

void PointShadowPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_pointShadowRt || !m_shadowPipeline || !m_meshResolver || !m_casterList ||
        m_pointFaceViewProjs.empty())
        return;

    RenderAttachmentOps ops{};
    ops.hasDepth = m_pointShadowRt->GetDepthTexture() != nullptr;
    ops.depth.loadOp = AttachmentLoadOp::Clear;
    ops.depth.storeOp = AttachmentStoreOp::Store;
    ops.depth.clearValue.depth = 1.0f;
    ops.depth.clearValue.stencil = 0;
    if (m_pointShadowRt->GetColorTexture(0) != nullptr) {
        ops.color.loadOp = AttachmentLoadOp::Clear;
        ops.color.storeOp = AttachmentStoreOp::Store;
        ops.color.clearValue.rgba = {1.0f, 1.0f, 1.0f, 1.0f};
    }
    if (!ops.hasDepth && m_pointShadowRt->GetColorTexture(0) == nullptr)
        return;

    const uint32_t w = m_pointShadowRt->GetWidth();
    const uint32_t h = m_pointShadowRt->GetHeight();

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetCullFace(true);
    // Point-light contact tuning: slightly lower hardware caster bias to reduce
    // contact detachment on near receiver surfaces.
    cmd->SetDepthBias(0.25f, 0.8f);
    cmd->BindPipeline(m_shadowPipeline);

    rhi::IBuffer* lastVB = nullptr;
    rhi::IBuffer* lastIB = nullptr;

    const uint32_t layerCount = static_cast<uint32_t>(m_pointFaceViewProjs.size());
    for (uint32_t layer = 0; layer < layerCount; ++layer) {
        m_pointShadowRt->SetDepthLayer(layer);
        cmd->SetRenderTarget(m_pointShadowRt, ops);
        if (w > 0 && h > 0)
            cmd->SetViewport(0, 0, w, h);

        math::Frustum lightFrustum;
        lightFrustum.FromMatrix(m_pointFaceViewProjs[layer]);

        if (m_shadowDataSet) {
            cmd->BindDescriptorSets(1, 1, &m_shadowDataSet);
        }

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

            if (lastVB != hMesh.vertexBuffer) {
                cmd->BindVertexBuffer(hMesh.vertexBuffer, 0, 0);
                lastVB = hMesh.vertexBuffer;
            }
            if (lastIB != hMesh.indexBuffer) {
                cmd->BindIndexBuffer(hMesh.indexBuffer, hMesh.indexType, 0);
                lastIB = hMesh.indexBuffer;
            }

            ShadowPushConstants pc{};
            pc.lightViewProj = m_pointFaceViewProjs[layer];
            pc.baseObjectIndex = i;
            cmd->SetPushConstants(0, sizeof(ShadowPushConstants), &pc);

            if (!hMesh.submeshes.empty()) {
                for (const Submesh& sm : hMesh.submeshes)
                    cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
            } else {
                cmd->DrawIndexed(hMesh.totalIndexCount, 0, 0);
            }
        }

        // Keep Clear for every array layer. Each layer is a separate depth slice and
        // must be reset every frame to avoid stale depth from prior frames.
    }
}

}  // namespace rendering
}  // namespace eyelib
