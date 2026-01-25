#include "api/render/passes/TransparencyPass.h"
#include "eyelib/render/passes/TransparencyPass.h"

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

void TransparencyPass::Render(const std::vector<Drawable>& transparent,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& viewPos) {
    std::vector<render::Drawable> internal;
    internal.reserve(transparent.size());
    for (const auto& d : transparent) {
        internal.push_back(ToInternal(d));
    }
    render::TransparencyPass::Render(internal, view, projection, viewPos);
}

} // namespace api
} // namespace eyelib
