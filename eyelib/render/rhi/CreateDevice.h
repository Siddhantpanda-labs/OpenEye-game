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

/** Create Diligent RHI device. nwh = native window handle (e.g. HWND on Windows). Stub implementation when Diligent Engine is not linked. */
std::unique_ptr<IDevice> CreateDiligentDevice(void* nwh, uint32_t width, uint32_t height);

} // namespace rhi
} // namespace render
} // namespace eyelib
