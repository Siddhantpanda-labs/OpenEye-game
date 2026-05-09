#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/resources/Mesh.h"
#include <vector>

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;
namespace rhi {
struct IRenderTarget;
struct IPipeline;
}

/**
 * P7.5 Point-light cubemap shadow pass.
 * Renders opaque/alpha-test casters into a depth2DArray: 6 layers per shadow-casting point light.
 * Uses the same depth-only pipeline as directional/spot (`raster/shadow/directional_depth`).
 */
class PointShadowPass final : public IFrameGraphPass {
public:
    PointShadowPass() = default;
    ~PointShadowPass() override = default;

    const char* DebugName() const noexcept override { return "PointShadowPass"; }

    void Setup(rhi::IRenderTarget* pointShadowRt,
               rhi::IPipeline* shadowPipeline,
               const IMeshHandleResolver* meshResolver,
               const ShadowCasterList* casterList,
               const std::vector<math::Mat4>& pointFaceViewProjs) noexcept;

    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                     RenderAttachmentOps& outOps) const override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    rhi::IRenderTarget* m_pointShadowRt = nullptr;
    rhi::IPipeline* m_shadowPipeline = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    const ShadowCasterList* m_casterList = nullptr;
    std::vector<math::Mat4> m_pointFaceViewProjs;
};

}  // namespace rendering
}  // namespace eyelib
