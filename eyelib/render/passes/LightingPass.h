#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/GBuffer.h"
#include <memory>
#include <span>

namespace eyelib {
namespace api {
namespace render {
struct DirectionalLight;
struct PointLight;
struct SpotLight;
}
}
namespace render {

class Shader;

class ShadowMap;

/** Deferred lighting: reads G-buffer, outputs lit HDR color to current FBO. Optional shadow map. */
class LightingPass {
public:
    LightingPass();
    ~LightingPass() = default;

    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const { return m_lightingShader; }

    /** Render to currently bound FBO. Binds GBuffer to units 0–4. If shadowMap non-null, binds shadow to unit 5 and applies shadows. */
    void Render(GBuffer& gbuffer,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                ShadowMap* shadowMap = nullptr,
                const std::vector<PointLight>* pointLights = nullptr,
                const std::vector<SpotLight>* spotLights = nullptr);

    /** Zero-copy: same but with API light types (same field layout). Accepts any contiguous container via span. */
    void Render(GBuffer& gbuffer,
                const api::render::DirectionalLight& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos,
                ShadowMap* shadowMap = nullptr,
                std::span<const api::render::PointLight> pointLights = {},
                std::span<const api::render::SpotLight> spotLights = {});

private:
    void EnsureDefaultShader();

    std::shared_ptr<Shader> m_lightingShader;
};

} // namespace render
} // namespace eyelib
