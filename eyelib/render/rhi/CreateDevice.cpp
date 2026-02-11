#include "eyelib/render/rhi/CreateDevice.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"
#include "eyelib/render/rhi/diligent/CreateDeviceDiligent.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/platform/window/GraphicsDevice.h"
#include "eyelib/platform/window/Window.h"
#include "eyelib/core/Log.h"

#if OPENEYE_USE_BGFX
#include "eyelib/render/rhi/bgfx/CreateDeviceBGFX.h"
#endif

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
    const char* backendName = (backend == render::RenderBackend::BGFX) ? "BGFX" :
                              (backend == render::RenderBackend::Diligent) ? "Diligent" :
                              (backend == render::RenderBackend::OpenGL) ? "OpenGL" : "Default";
    core::Log::InfoF("CreateGraphicsDevice: requested backend=%s width=%u height=%u", backendName, width, height);

    if (!window) {
        core::Log::Error("CreateGraphicsDevice: window is null");
        return nullptr;
    }

    const bool useDiligent = (backend == render::RenderBackend::Diligent);
    if (useDiligent) {
        core::Log::Info("CreateGraphicsDevice: creating Diligent device (stub if engine not linked)");
        void* nwh = window->GetNativeWindowHandle();
        if (!nwh) core::Log::Error("CreateGraphicsDevice: GetNativeWindowHandle() returned null");
        return render::rhi::CreateDiligentDevice(nwh, width, height);
    }

    const bool useBGFX = (backend == render::RenderBackend::BGFX);
#if OPENEYE_USE_BGFX
    if (useBGFX) {
        core::Log::Info("CreateGraphicsDevice: OPENEYE_USE_BGFX defined, creating BGFX device");
        void* nwh = window->GetNativeWindowHandle();
        if (!nwh) core::Log::Error("CreateGraphicsDevice: GetNativeWindowHandle() returned null");
        auto device = render::rhi::CreateBGFXDevice(nwh, static_cast<int>(width), static_cast<int>(height));
        if (device)
            core::Log::Info("CreateGraphicsDevice: BGFX device created");
        else
            core::Log::Error("CreateGraphicsDevice: CreateBGFXDevice returned null");
        return device;
    }
#endif
    if (useBGFX)
        core::Log::Warn("CreateGraphicsDevice: BGFX requested but OPENEYE_USE_BGFX not built; using OpenGL");
    core::Log::Info("CreateGraphicsDevice: creating OpenGL device");
    (void)width;
    (void)height;
    window->MakeContextCurrent();
    return render::rhi::CreateOpenGLDevice();
}

} // namespace platform
} // namespace eyelib
