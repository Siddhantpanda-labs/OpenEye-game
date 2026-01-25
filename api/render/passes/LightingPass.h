#pragma once

#include "api/render/GBuffer.h"
#include "api/render/RenderScene.h"
#include "api/render/Shader.h"
#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace render { class LightingPass; }
namespace api {

class LightingPass {
public:
    LightingPass();
    ~LightingPass();

    LightingPass(const LightingPass&) = delete;
    LightingPass& operator=(const LightingPass&) = delete;
    LightingPass(LightingPass&& other) noexcept;
    LightingPass& operator=(LightingPass&& other) noexcept;

    void SetLightingShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetLightingShader() const;

    void Render(GBuffer& gbuffer,
                const DirectionalLight& light,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& viewPos);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
