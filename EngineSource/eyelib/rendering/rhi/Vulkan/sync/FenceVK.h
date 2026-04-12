#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** Fence backed by device timeline semaphore; signals when the submit that contained InsertFence() completes. */
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
