#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

#include <string>
#include <utility>
#include <vector>

namespace eyelib {
namespace render {

class DebugDrawRenderer {
public:
    /** Release all per-device GPU resources. Call before destroying any device that was used for Render/RenderLinesWithDepth. */
    static void Shutdown();
    /** Render debug lines/text (context-based). */
    static void Render(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height);
    /** Render debug lines/text; device must be the same device used for the active render target. */
    static void Render(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height);
    /** Draw world-space line list with depth test on (context-based). */
    static void RenderLinesWithDepth(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color);
    /** Draw world-space line list with depth test on (occluded by geometry). */
    static void RenderLinesWithDepth(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color);
    /** Draw world-space lines with per-line color (context-based). */
    static void RenderLinesWithDepth(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors);
    /** Draw world-space lines with per-line color (depth test on). */
    static void RenderLinesWithDepth(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
        const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors);
    static void SetTextFontPath(const std::string& path, float pixelHeight = 16.0f);
    static void SetTextSize(float pixelHeight);
};

} // namespace render
} // namespace eyelib
