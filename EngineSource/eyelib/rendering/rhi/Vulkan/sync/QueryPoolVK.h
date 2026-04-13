#pragma once

#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

struct QueryPoolVK : IQueryPool {
    explicit QueryPoolVK(DeviceVK* device);
    ~QueryPoolVK() override;

    QueryType GetType() const override;
    uint32_t GetCount() const override;
    uint64_t GetResult(uint32_t index) override;

    DeviceVK* m_device = nullptr;
    QueryType m_type = QueryType::Timestamp;
    uint32_t m_count = 0;
    void* m_queryPool = nullptr;
    /** Nanoseconds per timestamp counter tick (`VkPhysicalDeviceLimits::timestampPeriod`). */
    double m_timestampPeriodNs = 1.0;
};

IQueryPool* CreateQueryPoolVK(DeviceVK* device, QueryType type, uint32_t count);
void DestroyQueryPoolVK(IQueryPool* pool);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
