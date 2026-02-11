#include "eyelib/render/rhi/bgfx/CreateDeviceBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/core/Log.h"

namespace eyelib {
namespace render {
namespace rhi {

std::unique_ptr<IDevice> CreateBGFXDevice(void* nwh, int width, int height) {
    core::Log::InfoF("CreateBGFXDevice: nwh=%p width=%d height=%d", nwh, width, height);

    if (!nwh) {
        core::Log::Error("CreateBGFXDevice: native window handle (nwh) is null");
        return nullptr;
    }
    if (width <= 0 || height <= 0) {
        core::Log::ErrorF("CreateBGFXDevice: invalid size width=%d height=%d", width, height);
        return nullptr;
    }

    return std::make_unique<DeviceBGFX>(nwh, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

} // namespace rhi
} // namespace render
} // namespace eyelib
