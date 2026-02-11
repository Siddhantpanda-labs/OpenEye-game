#include "eyelib/render/rhi/diligent/CreateDeviceDiligent.h"
#include "eyelib/render/rhi/diligent/DeviceDiligent.h"
#include "eyelib/core/Log.h"

#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Platforms/Win32/interface/Win32NativeWindow.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "eyelib/render/rhi/diligent/DeviceDiligentImpl.h"
#endif

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateDiligentDevice(void* nwh, uint32_t width, uint32_t height) {
    if (!nwh || width == 0 || height == 0) {
        core::Log::Error("CreateDiligentDevice: invalid window or size");
        return nullptr;
    }

#if OPENEYE_USE_DILIGENT
    using namespace Diligent;
    IEngineFactoryVk* pFactoryVk = GetEngineFactoryVk();
    if (!pFactoryVk) {
        core::Log::Error("CreateDiligentDevice: GetEngineFactoryVk failed");
        return std::make_unique<DeviceDiligent>(nwh, width, height, nullptr);
    }

    EngineVkCreateInfo engineCI;
    engineCI.SetValidationLevel(VALIDATION_LEVEL_DISABLED);
    RefCntAutoPtr<IRenderDevice> pDevice;
    RefCntAutoPtr<IDeviceContext> pContext;
    pFactoryVk->CreateDeviceAndContextsVk(engineCI, &pDevice, &pContext);
    if (!pDevice || !pContext) {
        core::Log::Error("CreateDiligentDevice: CreateDeviceAndContextsVk failed");
        return std::make_unique<DeviceDiligent>(nwh, width, height, nullptr);
    }

    SwapChainDesc scDesc;
    scDesc.Width = width;
    scDesc.Height = height;
    Win32NativeWindow nativeWindow(nwh);
    RefCntAutoPtr<ISwapChain> pSwapChain;
    pFactoryVk->CreateSwapChainVk(pDevice, pContext, scDesc, nativeWindow, &pSwapChain);
    if (!pSwapChain) {
        core::Log::Error("CreateDiligentDevice: CreateSwapChainVk failed");
        return std::make_unique<DeviceDiligent>(nwh, width, height, nullptr);
    }

    auto* impl = new DeviceDiligentImpl{};
    impl->pDevice = pDevice;
    impl->pContext = pContext;
    impl->pSwapChain = pSwapChain;
    // Diligent Vk swap chain acquires the next image inside Present(). Call once so the first
    // frame has an acquired image to clear and present; otherwise the first frame never presents.
    pSwapChain->Present(1);
    return std::make_unique<DeviceDiligent>(nwh, width, height, impl);
#else
    core::Log::InfoF("CreateDiligentDevice: nwh=%p width=%u height=%u (stub backend)", nwh, width, height);
    return std::make_unique<DeviceDiligent>(nwh, width, height, nullptr);
#endif
}

} // namespace rhi
} // namespace render
} // namespace eyelib
