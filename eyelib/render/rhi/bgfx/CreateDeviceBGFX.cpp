#include "eyelib/render/rhi/bgfx/CreateDeviceBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateBGFXDevice(GLFWwindow* glfwWindow, int width, int height) {
    if (!glfwWindow || width <= 0 || height <= 0) return nullptr;
    void* nwh = nullptr;
#if BX_PLATFORM_WINDOWS
    nwh = glfwGetWin32Window(glfwWindow);
#elif BX_PLATFORM_LINUX
    nwh = (void*)(uintptr_t)glfwGetX11Window(glfwWindow);
#elif BX_PLATFORM_OSX
    nwh = glfwGetCocoaWindow(glfwWindow);
#endif
    if (!nwh) return nullptr;
    return std::make_unique<DeviceBGFX>(nwh, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

} // namespace rhi
} // namespace render
} // namespace eyelib
