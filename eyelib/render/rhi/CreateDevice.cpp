#include "eyelib/render/rhi/CreateDevice.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/platform/window/Window.h"

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateOpenGLDevice() {
    return std::make_unique<DeviceGL>();
}

std::unique_ptr<IDevice> CreateGraphicsDevice(std::function<void()> makeContextCurrent) {
    if (makeContextCurrent) makeContextCurrent();
    return CreateOpenGLDevice();
}

} // namespace rhi
} // namespace render
} // namespace eyelib

namespace eyelib {
namespace platform {

std::unique_ptr<render::rhi::IDevice> CreateGraphicsDevice(Window* window) {
    if (!window) return nullptr;
    window->MakeContextCurrent();
    return render::rhi::CreateOpenGLDevice();
}

} // namespace platform
} // namespace eyelib
