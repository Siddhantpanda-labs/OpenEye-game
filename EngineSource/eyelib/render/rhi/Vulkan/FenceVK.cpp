#include "eyelib/render/rhi/Vulkan/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

FenceVK::FenceVK(DeviceVK* device, uint64_t timelineValue)
    : m_device(device), m_timelineValue(timelineValue) {}

FenceVK::~FenceVK() = default;

bool WaitFenceVK(DeviceVK* device, IFence* fence, uint64_t timeoutMs) {
    if (!device || !fence || !device->GetDevice() || !device->GetTimelineSemaphore()) return false;
    FenceVK* f = static_cast<FenceVK*>(fence);
    if (f->m_device != device) return false;
    VkSemaphore sem = static_cast<VkSemaphore>(device->GetTimelineSemaphore());
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &sem;
    waitInfo.pValues = &f->m_timelineValue;
    uint64_t timeoutNs = (timeoutMs == 0) ? 0 : (timeoutMs > (UINT64_MAX / 1000000) ? UINT64_MAX : timeoutMs * 1000000);
    VkResult res = vkWaitSemaphores(static_cast<VkDevice>(device->GetDevice()), &waitInfo, timeoutNs);
    return (res == VK_SUCCESS);
}

void DestroyFenceVK(IFence* fence) {
    if (fence) delete static_cast<FenceVK*>(fence);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
