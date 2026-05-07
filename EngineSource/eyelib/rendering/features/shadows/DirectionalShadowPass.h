#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/scene/DrawList.h"
#include <array>
#include <vector>

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;

struct ShadowCasterItem {
    uint32_t meshHandle = 0;
    math::Mat4 worldMatrix{1.0f};
    math::AABB worldBounds{};
    DrawLayer layer = DrawLayer::Opaque;
    uint32_t flags = 0;
};

struct ShadowCasterList {
    std::vector<ShadowCasterItem> items;
    void Clear() noexcept { items.clear(); }
    uint32_t Count() const noexcept { return static_cast<uint32_t>(items.size()); }
};

namespace rhi {
struct IPipeline;
struct IRenderTarget;
struct ITexture;
}

/**
 * P7 directional shadow map foundation pass.
 * Current scope: clear/write directional depth array layers for CSM groundwork.
 */
class DirectionalShadowPass final : public IFrameGraphPass {
public:
    DirectionalShadowPass(rhi::IRenderTarget* directionalShadowRt,
                          rhi::IPipeline* shadowPipeline,
                          const IMeshHandleResolver* meshResolver,
                          const ShadowCasterList* casterList,
                          const std::array<math::Mat4, 3>* lightViewProj) noexcept;

    const char* DebugName() const noexcept override { return pass_names::kDirectionalShadow; }

    void Reconfigure(rhi::IRenderTarget* directionalShadowRt,
                     rhi::IPipeline* shadowPipeline,
                     const IMeshHandleResolver* meshResolver,
                     const ShadowCasterList* casterList,
                     const std::array<math::Mat4, 3>* lightViewProj) noexcept;
    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt, RenderAttachmentOps& outOps) const override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    rhi::IRenderTarget* m_directionalShadowRt = nullptr;
    rhi::IPipeline* m_shadowPipeline = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    const ShadowCasterList* m_casterList = nullptr;
    const std::array<math::Mat4, 3>* m_lightViewProj = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
