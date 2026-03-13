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

    /** Render to currently bound FBO (context-based). If sceneLightingBuffer is non-null, it is bound at binding 5. cascadeMap: bind cascade depth array at 6, light-space SSBO at 7. ibl: when non-null and valid, binds irradiance/prefilter/BRDF at 11/12/13 and sets u_UseIBL. */
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

    /** Render to currently bound FBO. device and cmd must be non-null. ibl: when non-null and valid, binds IBL textures and sets u_UseIBL. */
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
};

} // namespace render
} // namespace eyelib
