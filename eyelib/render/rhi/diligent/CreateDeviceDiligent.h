#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

/** Create Diligent RHI device. nwh = native window handle (e.g. HWND on Windows).
 *  Returns stub device when Diligent Engine is not linked. */
std::unique_ptr<IDevice> CreateDiligentDevice(void* nwh, uint32_t width, uint32_t height);

} // namespace rhi
} // namespace render
} // namespace eyelib
