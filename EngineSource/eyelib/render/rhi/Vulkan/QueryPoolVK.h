#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render {
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
    // VkQueryPool (filled when implemented)
    void* m_queryPool = nullptr;
};

IQueryPool* CreateQueryPoolVK(DeviceVK* device, QueryType type, uint32_t count);
void DestroyQueryPoolVK(IQueryPool* pool);

} // namespace rhi
} // namespace render
} // namespace eyelib
