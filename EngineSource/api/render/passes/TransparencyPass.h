#pragma once

#include "api/render/RenderScene.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render { class ShaderCache; }

namespace api {

class TransparencyPass {
public:
    /** device and shaderCache required for unified material path; when null, no draw. */
    static void Render(::eyelib::render::rhi::ICommandList* cmd,
                       ::eyelib::render::rhi::IDevice* device,
                       const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos,
                       ::eyelib::render::ShaderCache* shaderCache);
};

} // namespace api
} // namespace eyelib
