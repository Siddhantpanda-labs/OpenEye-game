#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Fence backed by device timeline semaphore; signals when the submit that contained InsertFence() completes. */
struct FenceVK : IFence {
    explicit FenceVK(DeviceVK* device, uint64_t timelineValue);
    ~FenceVK() override;

    DeviceVK* m_device = nullptr;
    uint64_t m_timelineValue = 0;  // wait until semaphore >= this value
};

} // namespace rhi
} // namespace render
} // namespace eyelib
