#pragma once

#include "api/render/RenderScene.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace api {

class TransparencyPass {
public:
    /** cmd used for blend/depth state; may be null. */
    static void Render(::eyelib::render::rhi::ICommandList* cmd,
                       const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos);
};

} // namespace api
} // namespace eyelib
