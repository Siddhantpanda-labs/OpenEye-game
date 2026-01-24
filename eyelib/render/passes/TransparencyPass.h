#pragma once

#include "eyelib/render/RenderTypes.h"
#include <vector>

namespace eyelib {
namespace render {

/** Forward-renders transparent drawables; back-to-front sorted by distance from camera. */
class TransparencyPass {
public:
    TransparencyPass() = default;

    /** Sort by distance (back-to-front) and render with alpha blending. */
    static void Render(const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos);
};

} // namespace render
} // namespace eyelib
