#pragma once

#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/math/Types.h"
#include <array>
#include <memory>
#include <vector>

namespace eyelib {
namespace rendering {

class DirectionalShadowPass;
class SpotShadowPass;
class FrameGraph;
class FrameGraphBlackboard;
struct ShadowCasterList;
namespace rhi {
struct IRenderTarget;
struct IBuffer;
struct IDevice;
struct IPipeline;
}

/**
 * P7 scaffold owner for shadow scheduling/contracts.
 * Initial responsibility: reserve/release shadow blackboard slots and publish
 * directional shadow foundations (depth array + params buffer).
 */
class ShadowFeature final : public IRenderFeature {
public:
    ShadowFeature();
    ~ShadowFeature() override;
    void SetMeshResolver(const IMeshHandleResolver* resolver) noexcept { m_meshResolver = resolver; }

    const char* FeatureDebugName() const noexcept override { return writer_names::kShadowFeature; }
    void OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) override;
    void OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

private:
    rhi::IRenderTarget* m_directionalShadowRt = nullptr;
    rhi::IBuffer* m_shadowParamsBuffer = nullptr;
    rhi::IPipeline* m_shadowPipeline = nullptr;
    rhi::IPipeline* m_spotShadowPipeline = nullptr;
    rhi::IDevice* m_device = nullptr;
    const IMeshHandleResolver* m_meshResolver = nullptr;
    ShadowCasterList* m_shadowCasterList = nullptr;
    std::unique_ptr<ShadowCasterList> m_ownedShadowCasterList;
    std::array<math::Mat4, 3> m_lightViewProj = {math::Mat4{1.0f}, math::Mat4{1.0f}, math::Mat4{1.0f}};
    std::unique_ptr<DirectionalShadowPass> m_directionalShadowPass;
    rhi::IRenderTarget* m_spotShadowRt = nullptr;
    rhi::IBuffer* m_spotLightUniformBuffer = nullptr;
    std::unique_ptr<SpotShadowPass> m_spotShadowPass;
    std::vector<math::Mat4> m_spotShadowViewProjs;
    uint32_t m_spotShadowLayerCount = 0u;
};

}  // namespace rendering
}  // namespace eyelib
