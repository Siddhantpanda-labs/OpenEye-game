#pragma once

#include "eyelib/rendering/features/raster/SkyboxRequest.h"
#include "eyelib/rendering/framegraph/FrameGraphAttachmentOps.h"
#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include "eyelib/rendering/framegraph/FrameGraphPassNames.h"
#include "eyelib/math/Types.h"

#include <cstdint>

namespace eyelib {
namespace rendering {

class FrameGraphBlackboard;
struct FrameGraphBarrierList;
namespace rhi {
struct ICommandList;
struct IDescriptorSet;
struct IDevice;
struct IPipeline;
struct IRenderTarget;
struct ITexture;
}

/**
 * Frame-graph pass that draws the sky over `MainHdrRenderTarget` (color0) with depth
 * test enabled and depth write off. Renders a single full-screen triangle whose far-
 * plane direction is reconstructed in the shader from `uInverseView` / `uInverseProjection`
 * push constants (set by the pass at Execute time from the active `PerView` matrices).
 *
 * Owns:
 *  - A triple-buffered descriptor set bound to set 0 binding 0 = samplerCube (only used
 *    in `SkyboxRequest::Kind::Cubemap`). For procedural sky, no descriptor sets bind.
 *
 * Does NOT own:
 *  - The cubemap texture (`SkyboxRequest::cubemap`). The caller (public `Skybox` handle
 *    routed through `RenderPipeline::Render`) owns the underlying GPU resource and keeps
 *    it alive at least until the next `Render` call.
 *  - The render target — published by `RenderResources` on the `MainHdrRenderTarget` slot.
 *
 * Run order: after `RasterFeature` (so the lighting pass clears + writes color), before
 * `EditorFeature::DebugDrawPass` (so debug overlays land on top of the sky). The
 * `SkyboxFeature` returns `SchedulingOrder() = 50` to enforce this in the feature graph.
 */
class SkyboxPass final : public IFrameGraphPass {
public:
    SkyboxPass(FrameGraphBlackboard* blackboard,
               rhi::IDevice* device,
               rhi::IPipeline* cubemapPipeline,
               rhi::IPipeline* proceduralPipeline) noexcept;
    ~SkyboxPass() override;

    SkyboxPass(const SkyboxPass&) = delete;
    SkyboxPass& operator=(const SkyboxPass&) = delete;

    /** Refresh blackboard + device + pipelines (idempotent for the same values). Per-frame setup.
     *  The blackboard pointer is borrowed; the feature owns its lifetime and re-passes it every
     *  schedule call so the pass always reads from the live frame graph state. */
    void Reconfigure(FrameGraphBlackboard* blackboard,
                     rhi::IDevice* device,
                     rhi::IPipeline* cubemapPipeline,
                     rhi::IPipeline* proceduralPipeline) noexcept;

    /** Per-frame sky source + view matrices. `SkyboxFeature` calls this each schedule call
     *  with the request from the public `RenderScene` and inverse view/projection derived
     *  from the active camera. */
    void SetFrameState(const SkyboxRequest& request,
                       const math::Mat4& inverseView,
                       const math::Mat4& inverseProjection) noexcept {
        m_request = request;
        m_inverseView = inverseView;
        m_inverseProjection = inverseProjection;
    }

    const char* DebugName() const noexcept override { return pass_names::kSkybox; }
    void CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) override;
    bool DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                     RenderAttachmentOps& outOps) const override;
    void Execute(rhi::ICommandList* cmd) override;

private:
    static constexpr uint32_t kFrameSlots = 3;

    void DestroyDeviceResources() noexcept;

    FrameGraphBlackboard* m_blackboard          = nullptr;
    rhi::IDevice*         m_device              = nullptr;
    rhi::IPipeline*       m_cubemapPipeline     = nullptr;
    rhi::IPipeline*       m_proceduralPipeline  = nullptr;
    SkyboxRequest         m_request{};
    math::Mat4            m_inverseView{1.0f};
    math::Mat4            m_inverseProjection{1.0f};

    /** Per-in-flight-frame samplerCube binding (set 0 binding 0 in the IBL layout). */
    rhi::IDescriptorSet* m_cubemapSets[kFrameSlots]      = {nullptr, nullptr, nullptr};
    rhi::ITexture*       m_cubemapSetTextures[kFrameSlots] = {nullptr, nullptr, nullptr};
};

}  // namespace rendering
}  // namespace eyelib
