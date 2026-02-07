#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"

#include <string>

namespace eyelib {
namespace render {

class DebugDrawRenderer {
public:
    static void Render(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height);
    static void SetTextFontPath(const std::string& path, float pixelHeight = 16.0f);
    static void SetTextSize(float pixelHeight);
};

} // namespace render
} // namespace eyelib
