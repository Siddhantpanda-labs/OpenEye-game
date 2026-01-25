#pragma once

#include "api/render/RenderScene.h"
#include "eyelib/math/Types.h"
#include <vector>

namespace eyelib {
namespace api {

class TransparencyPass {
public:
    static void Render(const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos);
};

} // namespace api
} // namespace eyelib
