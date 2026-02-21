#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/GBuffer.h"
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

    void SetDevice(rhi::IDevice* device) { m_device = device; }
    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const { return m_lightingShader; }

    /** Render to currently bound FBO. cmd used for shadow bind and depth state (GL-free). */
    void Render(GBuffer& gbuffer,
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
    void EnsureDefaultShader();

    std::shared_ptr<Shader> m_lightingShader;
    rhi::IDevice* m_device = nullptr;
};

} // namespace render
} // namespace eyelib
