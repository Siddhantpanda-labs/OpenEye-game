#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render {

class ShaderCache;

/** Forward-renders transparent drawables; back-to-front sorted by distance from camera. */
class TransparencyPass {
public:
    TransparencyPass() = default;

    /** Sort by distance (back-to-front) and render with alpha blending (context-based). Requires non-null shaderCache. */
    static void Render(const RenderContext& ctx,
                       const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos,
                       ShaderCache* shaderCache);
    /** Sort by distance (back-to-front) and render with alpha blending. shaderCache required for unified material shader path. */
    static void Render(rhi::ICommandList* cmd,
                       rhi::IDevice* device,
                       const std::vector<Drawable>& transparent,
                       const math::Mat4& view,
                       const math::Mat4& projection,
                       const math::Vec3& viewPos,
                       ShaderCache* shaderCache);
};

} // namespace render
} // namespace eyelib
