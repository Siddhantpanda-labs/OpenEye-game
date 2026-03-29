#pragma once

#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace render {

class Shader;
class CascadeShadowMap;
class IBL;

/** Deferred lighting: reads G-buffer, outputs lit HDR color to current FBO. Optional cascade shadow map, optional IBL. */
class LightingPass {
public:
    LightingPass();
    ~LightingPass();

    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const { return m_lightingShader; }
    /** Frame + DeferredLightingPass descriptor sets (required for Render; see descriptor_binding_table.md). */
    void SetResourceSpaceSets(rhi::IDescriptorSet* frameSet, rhi::IDescriptorSet* deferredLightingSet) {
        m_lightingFrameSet = frameSet;
        m_lightingDeferredPassSet = deferredLightingSet;
    }

    /** Render to currently bound FBO (context-based). G-buffer and lighting inputs come from descriptor sets (RenderPipeline updates). Sets u_UseIBL from ibl. */
    void Render(GBuffer& gbuffer,
                const RenderContext& ctx,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                CascadeShadowMap* cascadeMap = nullptr,
                const std::vector<PointLight>* pointLights = nullptr,
                const std::vector<SpotLight>* spotLights = nullptr,
                rhi::IBuffer* sceneLightingBuffer = nullptr,
                IBL* ibl = nullptr);

    /** Render to currently bound FBO. Requires cmd, lightingFrameSet, and lightingDeferredPassSet. */
    void Render(GBuffer& gbuffer,
                rhi::IDevice* device,
                rhi::ICommandList* cmd,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                CascadeShadowMap* cascadeMap = nullptr,
                const std::vector<PointLight>* pointLights = nullptr,
                const std::vector<SpotLight>* spotLights = nullptr,
                rhi::IBuffer* sceneLightingBuffer = nullptr,
                IBL* ibl = nullptr);

    /** Reload lighting shader (for hot-reload when shader files change). */
    void ReloadShaders();

private:
    void EnsureDefaultShader(rhi::IDevice* device);

    std::shared_ptr<Shader> m_lightingShader;
    rhi::IDescriptorSet* m_lightingFrameSet = nullptr;
    rhi::IDescriptorSet* m_lightingDeferredPassSet = nullptr;
};

} // namespace render
} // namespace eyelib
