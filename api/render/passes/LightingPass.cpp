#include "api/render/passes/LightingPass.h"
#include "eyelib/render/passes/LightingPass.h"

namespace eyelib {
namespace api {

class LightingPass::Impl {
public:
    std::unique_ptr<render::LightingPass> pass = std::make_unique<render::LightingPass>();
};

LightingPass::LightingPass() : m_impl(std::make_unique<Impl>()) {}

LightingPass::~LightingPass() = default;

LightingPass::LightingPass(LightingPass&& other) noexcept = default;

LightingPass& LightingPass::operator=(LightingPass&& other) noexcept = default;

void LightingPass::SetLightingShader(std::shared_ptr<Shader> shader) {
    m_impl->pass->SetLightingShader(shader ? shader->GetInternal() : nullptr);
}

std::shared_ptr<Shader> LightingPass::GetLightingShader() const {
    return Shader::WrapInternal(m_impl->pass->GetLightingShader());
}

void LightingPass::Render(GBuffer& gbuffer,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos) {
    render::LightParams internal;
    internal.direction = light.direction;
    internal.color = light.color;
    internal.intensity = light.intensity;
    internal.ambient = light.ambient;
    internal.exposure = light.exposure;

    m_impl->pass->Render(*gbuffer.GetInternal(), internal, view, projection, viewPos);
}

} // namespace api
} // namespace eyelib
