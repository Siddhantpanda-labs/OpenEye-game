#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>

struct GLFWwindow;

namespace eyelib {
namespace render {
namespace rhi {

/** Create BGFX RHI device. Call with GLFW window and size; bgfx is initialized with platform nwh. */
std::unique_ptr<IDevice> CreateBGFXDevice(GLFWwindow* glfwWindow, int width, int height);

} // namespace rhi
} // namespace render
} // namespace eyelib
