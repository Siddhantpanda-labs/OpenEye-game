#include "api/render/passes/GBufferPass.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/RenderContext.h"

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

class GBufferPass::Impl {
public:
    std::unique_ptr<::eyelib::render::GBufferPass> pass = std::make_unique<::eyelib::render::GBufferPass>();
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
    std::vector<::eyelib::render::Drawable> internal;
    internal.reserve(drawables.size());
    for (const auto& d : drawables) {
        internal.push_back(ToInternal(d));
    }
    auto ctx = m_device ? ::eyelib::render::RenderContext::FromDevice(static_cast<::eyelib::render::rhi::IDevice*>(m_device.Get())) : ::eyelib::render::RenderContext{};
    m_impl->pass->Render(*gbuffer.GetInternal(), ctx, internal, view, projection, prevView, prevProjection);
}

} // namespace api
} // namespace eyelib
