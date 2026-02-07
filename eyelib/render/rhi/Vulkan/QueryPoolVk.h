#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vulkan/vulkan_core.h>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;

struct QueryPoolVk : IQueryPool {
    QueryType GetType() const override;
    uint32_t GetCount() const override;
    uint64_t GetResult(uint32_t index) override;

    static QueryPoolVk* Create(DeviceVk* device, QueryType type, uint32_t count);
    static void Destroy(DeviceVk* device, QueryPoolVk* pool);

    VkQueryPool m_pool = VK_NULL_HANDLE;
    QueryType m_type = QueryType::Timestamp;
    uint32_t m_count = 0;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
