#pragma once

#include "eyelib/render/RenderTypes.h"

#include <string>

namespace eyelib {
namespace render {

class DebugDrawRenderer {
public:
    static void Render(const CameraView& camera, unsigned int targetFbo, unsigned int width, unsigned int height);
    static void SetTextFontPath(const std::string& path, float pixelHeight = 16.0f);
    static void SetTextSize(float pixelHeight);
};

} // namespace render
} // namespace eyelib
