#include "api/render/passes/ShadowPass.h"
#include "eyelib/render/passes/ShadowPass.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::Drawable ToInternal(const Drawable& d) {
    ::eyelib::render::Drawable out;
    out.mesh = d.mesh ? d.mesh->GetInternal() : nullptr;
    out.material = d.material ? d.material->GetInternal() : nullptr;
    out.transform = d.transform;
    return out;
}
} // namespace

class ShadowPass::Impl {
public:
    std::unique_ptr<::eyelib::render::ShadowPass> pass = std::make_unique<::eyelib::render::ShadowPass>();
};

ShadowPass::ShadowPass() : m_impl(std::make_unique<Impl>()) {}

ShadowPass::~ShadowPass() = default;

ShadowPass::ShadowPass(ShadowPass&& other) noexcept = default;

ShadowPass& ShadowPass::operator=(ShadowPass&& other) noexcept = default;

void ShadowPass::SetDepthShader(std::shared_ptr<Shader> shader) {
    m_impl->pass->SetDepthShader(shader ? shader->GetInternal() : nullptr);
}

std::shared_ptr<Shader> ShadowPass::GetDepthShader() const {
    return Shader::WrapInternal(m_impl->pass->GetDepthShader());
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        const std::vector<Drawable>& drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    std::vector<::eyelib::render::Drawable> internal;
    internal.reserve(drawables.size());
    for (const auto& d : drawables) {
        internal.push_back(ToInternal(d));
    }
    m_impl->pass->Render(*shadowMap.GetInternal(), nullptr, internal, lightDir, view, projection, cameraPos, nearPlane, farPlane);
}

} // namespace api
} // namespace eyelib
