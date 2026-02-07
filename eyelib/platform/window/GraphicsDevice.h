#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace platform {

class Window;

/** Create an RHI device for the given window. Makes the window's GL context current, then creates the OpenGL device. Implemented in the render module. */
std::unique_ptr<render::rhi::IDevice> CreateGraphicsDevice(Window* window);

} // namespace platform
} // namespace eyelib
