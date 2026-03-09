#pragma once

#include "eyelib/render/GBuffer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace render {

class Shader;

class ShadowMap;

/** Deferred lighting: reads G-buffer, outputs lit HDR color to current FBO. Optional shadow map. */
class LightingPass {
public:
    LightingPass();
    ~LightingPass();

    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const { return m_lightingShader; }

    /** Render to currently bound FBO (context-based). */
    void Render(GBuffer& gbuffer,
                const RenderContext& ctx,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                ShadowMap* shadowMap = nullptr,
                const std::vector<PointLight>* pointLights = nullptr,
                const std::vector<SpotLight>* spotLights = nullptr);

    /** Render to currently bound FBO. device and cmd must be non-null. */
    void Render(GBuffer& gbuffer,
                rhi::IDevice* device,
                rhi::ICommandList* cmd,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                ShadowMap* shadowMap = nullptr,
                const std::vector<PointLight>* pointLights = nullptr,
                const std::vector<SpotLight>* spotLights = nullptr);

    /** Reload lighting shader (for hot-reload when shader files change). */
    void ReloadShaders();

private:
    void EnsureDefaultShader(rhi::IDevice* device);

    std::shared_ptr<Shader> m_lightingShader;
};

} // namespace render
} // namespace eyelib
