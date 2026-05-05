#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** CPU wait object implemented with the device **timeline semaphore** (`DeviceVK::GetTimelineSemaphore`), not a `VkFence`.
 *  `InsertFence` / `ReserveTimelineValue` pairs a monotonic value; `WaitFence` uses `vkWaitSemaphores` on that value.
 *  This path is separate from **per-frame** `VkFence` in `FrameContextVK` (used by `BeginFrame`/`EndFrame`). */
struct FenceVK : IFence {
    explicit FenceVK(DeviceVK* device, uint64_t timelineValue);
    ~FenceVK() override;

    DeviceVK* m_device = nullptr;
    uint64_t m_timelineValue = 0;  // wait until semaphore >= this value
};

bool WaitFenceVK(DeviceVK* device, IFence* fence, uint64_t timeoutMs);
void DestroyFenceVK(IFence* fence);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
