#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

/** Create BGFX RHI device. nwh = native window handle (e.g. HWND on Windows). */
std::unique_ptr<IDevice> CreateBGFXDevice(void* nwh, int width, int height);

} // namespace rhi
} // namespace render
} // namespace eyelib
