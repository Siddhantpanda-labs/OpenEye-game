#include "eyelib/render/rhi/Vulkan/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"

namespace eyelib {
namespace render {
namespace rhi {

FenceVK::FenceVK(DeviceVK* device, uint64_t timelineValue)
    : m_device(device), m_timelineValue(timelineValue) {}

FenceVK::~FenceVK() = default;

} // namespace rhi
} // namespace render
} // namespace eyelib
