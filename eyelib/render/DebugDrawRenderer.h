#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

#include <string>
#include <utility>
#include <vector>

namespace eyelib {
namespace render {

class DebugDrawRenderer {
public:
    static void SetDevice(rhi::IDevice* device);
    static void Render(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height);
    /** Draw world-space line list with depth test on (occluded by geometry). */
    static void RenderLinesWithDepth(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color);
    static void SetTextFontPath(const std::string& path, float pixelHeight = 16.0f);
    static void SetTextSize(float pixelHeight);
};

} // namespace render
} // namespace eyelib
