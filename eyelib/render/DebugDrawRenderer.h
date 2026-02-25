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
    /** Release static GPU resources (shader, buffers). Call before destroying the device they were created with. */
    static void Shutdown();
    static void Render(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height);
    /** Draw world-space line list with depth test on (occluded by geometry). */
    static void RenderLinesWithDepth(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color);
    /** Draw world-space lines with per-line color (depth test on). */
    static void RenderLinesWithDepth(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors);
    static void SetTextFontPath(const std::string& path, float pixelHeight = 16.0f);
    static void SetTextSize(float pixelHeight);
};

} // namespace render
} // namespace eyelib
