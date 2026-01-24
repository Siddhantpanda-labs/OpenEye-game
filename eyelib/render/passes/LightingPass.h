#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/GBuffer.h"
#include <memory>

namespace eyelib {
namespace render {

class Shader;

/** Deferred lighting: reads G-buffer, outputs lit HDR color to current FBO. */
class LightingPass {
public:
    LightingPass();
    ~LightingPass() = default;

    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const { return m_lightingShader; }

    /** Render to currently bound FBO (viewport must be set). Binds GBuffer textures to units 0–4. */
    void Render(GBuffer& gbuffer,
                const LightParams& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos);

private:
    void EnsureDefaultShader();

    std::shared_ptr<Shader> m_lightingShader;
};

} // namespace render
} // namespace eyelib
