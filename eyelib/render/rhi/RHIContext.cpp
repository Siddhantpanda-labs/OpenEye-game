#include "eyelib/render/rhi/RHIContext.h"

namespace eyelib {
namespace render {
namespace rhi {

namespace {
IDevice* g_defaultDevice = nullptr;
}

void SetDefaultDevice(IDevice* device) {
    g_defaultDevice = device;
}

IDevice* GetDefaultDevice() {
    return g_defaultDevice;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
