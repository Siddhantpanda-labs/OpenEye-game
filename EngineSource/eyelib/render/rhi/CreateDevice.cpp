#include "eyelib/render/rhi/CreateDevice.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#if OPENEYE_VULKAN_AVAILABLE
#include "eyelib/render/rhi/Vulkan/CreateDeviceVulkan.h"
#endif
#if OPENEYE_USE_DILIGENT
#include "eyelib/render/rhi/diligent/CreateDeviceDiligent.h"
#endif
#include "eyelib/render/RenderBackend.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/platform/window/Window.h"
#include "eyelib/core/Log.h"

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

std::unique_ptr<render::rhi::IDevice> CreateGraphicsDevice(
    Window* window,
    uint32_t width,
    uint32_t height,
    render::RenderBackend backend) {
    const char* backendName = (backend == render::RenderBackend::Diligent) ? "Diligent" :
                              (backend == render::RenderBackend::Vulkan) ? "Vulkan" :
                              (backend == render::RenderBackend::OpenGL) ? "OpenGL" : "Default";
    core::Log::InfoF("CreateGraphicsDevice: requested backend=%s width=%u height=%u", backendName, width, height);

    if (!window) {
        core::Log::Error("CreateGraphicsDevice: window is null");
        return nullptr;
    }

#if OPENEYE_VULKAN_AVAILABLE
    if (backend == render::RenderBackend::Vulkan) {
        void* glfwWindow = static_cast<void*>(window->NativeHandle());
        if (!glfwWindow) { core::Log::Error("CreateGraphicsDevice: NativeHandle() returned null"); return nullptr; }
        try {
            auto device = render::rhi::CreateVulkanDevice(glfwWindow, width, height);
            if (!device) core::Log::Error("CreateGraphicsDevice: Vulkan device creation failed");
            return device;
        } catch (const std::exception& e) {
            core::Log::ErrorF("CreateGraphicsDevice: Vulkan threw: %s", e.what());
            return nullptr;
        } catch (...) {
            core::Log::Error("CreateGraphicsDevice: Vulkan threw unknown exception");
            return nullptr;
        }
    }
#else
    if (backend == render::RenderBackend::Vulkan) {
        core::Log::Error("CreateGraphicsDevice: Vulkan requested but Vulkan SDK not available (build without Vulkan)");
        return nullptr;
    }
#endif

#if OPENEYE_USE_DILIGENT
    const bool useDiligent = (backend == render::RenderBackend::Diligent);
    if (useDiligent) {
        core::Log::Info("CreateGraphicsDevice: creating Diligent device (stub if engine not linked)");
        void* nwh = window->GetNativeWindowHandle();
        if (!nwh) core::Log::Error("CreateGraphicsDevice: GetNativeWindowHandle() returned null");
        return render::rhi::CreateDiligentDevice(nwh, width, height);
    }
#endif

    (void)width;
    (void)height;
    window->MakeContextCurrent();
    return render::rhi::CreateOpenGLDevice();
}

} // namespace platform
} // namespace eyelib
