#include "eyelib/render/rhi/Vulkan/QueryPoolVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"

namespace eyelib {
namespace render {
namespace rhi {

QueryPoolVk* QueryPoolVk::Create(DeviceVk* device, QueryType type, uint32_t count) {
    if (!device || count == 0) return nullptr;
    VkQueryPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = count;
    auto* p = new QueryPoolVk();
    p->m_device = device;
    p->m_type = type;
    p->m_count = count;
    if (vkCreateQueryPool(device->GetDevice(), &info, nullptr, &p->m_pool) != VK_SUCCESS) { delete p; return nullptr; }
    return p;
}

void QueryPoolVk::Destroy(DeviceVk* device, QueryPoolVk* pool) {
    if (!device || !pool) return;
    if (pool->m_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device->GetDevice(), pool->m_pool, nullptr);
        pool->m_pool = VK_NULL_HANDLE;
    }
    delete pool;
}

QueryType QueryPoolVk::GetType() const { return m_type; }
uint32_t QueryPoolVk::GetCount() const { return m_count; }

uint64_t QueryPoolVk::GetResult(uint32_t index) {
    if (!m_device || m_pool == VK_NULL_HANDLE || index >= m_count) return 0;
    uint64_t result = 0;
    vkGetQueryPoolResults(m_device->GetDevice(), m_pool, index, 1, sizeof(result), &result, sizeof(result), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    return result;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
