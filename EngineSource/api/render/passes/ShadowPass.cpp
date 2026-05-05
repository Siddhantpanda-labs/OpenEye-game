#include "api/render/passes/ShadowPass.h"
#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"

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

void ShadowPass::Render(CascadeShadowMap& cascadeShadowMap,
                        const std::vector<Drawable>& drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane,
                        float aspectRatio) {
    std::vector<::eyelib::render::Drawable> internal;
    internal.reserve(drawables.size());
    for (const auto& d : drawables) {
        internal.push_back(ToInternal(d));
    }
    auto* device = m_device ? static_cast<::eyelib::render::rhi::IDevice*>(m_device.Get()) : nullptr;
    auto* cmd = device ? device->GetImmediateCommandList() : nullptr;
    if (!device || !cmd) return;
    ::eyelib::render::ShadowPass::DrawableFeed feed = [&internal](std::function<void(const ::eyelib::render::Drawable&)> c) {
        for (const auto& d : internal) c(d);
    };
    m_impl->pass->Render(*cascadeShadowMap.GetInternal(), device, cmd, std::move(feed), lightDir, view, projection, cameraPos, nearPlane, farPlane, aspectRatio);
}

} // namespace api
} // namespace eyelib
