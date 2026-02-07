#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <functional>
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

/** Create the OpenGL RHI device. Call with the GL context already current on the calling thread. */
std::unique_ptr<IDevice> CreateOpenGLDevice();

/** Make the GL context current (e.g. via window->MakeContextCurrent()), then create the device. */
std::unique_ptr<IDevice> CreateGraphicsDevice(std::function<void()> makeContextCurrent);

} // namespace rhi
} // namespace render
} // namespace eyelib
