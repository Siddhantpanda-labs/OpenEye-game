#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h" // ShadowCasterList, ShadowCasterItem
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/resources/Mesh.h" // MeshHandle, Submesh
#include <vector>

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;
namespace rhi {
struct IRenderTarget;
struct IPipeline;
}

/**
 * P7.4 Spot-light shadow pass.
 * Renders all opaque/alpha-test casters into each spot-light slice of the
 * SpotShadowMapAtlas depth array (Depth24 target). One layer per
 * shadow-casting spot light. Sampling path uses depth-only PCF.
 */
class SpotShadowPass final : public IFrameGraphPass {
public:
    SpotShadowPass() = default;
    ~SpotShadowPass() override = default;

    const char* DebugName() const noexcept override { return "SpotShadowPass"; }

    void Setup(rhi::IRenderTarget* spotShadowRt,
               rhi::IPipeline* shadowPipeline,
               const IMeshHandleResolver* meshResolver,
               const ShadowCasterList* casterList,
               const std::vector<math::Mat4>& spotLightViewProjs) noexcept;

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    rhi::IRenderTarget*          m_spotShadowRt = nullptr;
    rhi::IPipeline*              m_shadowPipeline = nullptr;
    const IMeshHandleResolver*   m_meshResolver = nullptr;
    const ShadowCasterList*      m_casterList = nullptr;
    std::vector<math::Mat4>      m_spotLightViewProjs;
};

}  // namespace rendering
}  // namespace eyelib
