#include "api/render/passes/GBufferPass.h"
#include "eyelib/render/passes/GBufferPass.h"

namespace eyelib {
namespace api {

namespace {
render::Drawable ToInternal(const Drawable& d) {
    render::Drawable out;
    out.mesh = d.mesh ? d.mesh->GetInternal() : nullptr;
    out.material = d.material ? d.material->GetInternal() : nullptr;
    out.transform = d.transform;
    return out;
}
} // namespace

class GBufferPass::Impl {
public:
    std::unique_ptr<render::GBufferPass> pass = std::make_unique<render::GBufferPass>();
};

GBufferPass::GBufferPass() : m_impl(std::make_unique<Impl>()) {}

GBufferPass::~GBufferPass() = default;

GBufferPass::GBufferPass(GBufferPass&& other) noexcept = default;

GBufferPass& GBufferPass::operator=(GBufferPass&& other) noexcept = default;

void GBufferPass::SetGeometryShader(std::shared_ptr<Shader> shader) {
    m_impl->pass->SetGeometryShader(shader ? shader->GetInternal() : nullptr);
}

std::shared_ptr<Shader> GBufferPass::GetGeometryShader() const {
    return Shader::WrapInternal(m_impl->pass->GetGeometryShader());
}

void GBufferPass::Render(GBuffer& gbuffer,
                         const std::vector<Drawable>& drawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) {
    std::vector<render::Drawable> internal;
    internal.reserve(drawables.size());
    for (const auto& d : drawables) {
        internal.push_back(ToInternal(d));
    }
    m_impl->pass->Render(*gbuffer.GetInternal(), internal, view, projection, prevView, prevProjection);
}

} // namespace api
} // namespace eyelib
