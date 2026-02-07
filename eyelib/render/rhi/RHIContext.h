#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Global default device for resource creation and immediate command list usage. */
void SetDefaultDevice(IDevice* device);
IDevice* GetDefaultDevice();

} // namespace rhi
} // namespace render
} // namespace eyelib
