#include "eyelib/render/rhi/Vulkan/QueryPoolVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"

namespace eyelib {
namespace render {
namespace rhi {

QueryPoolVK::QueryPoolVK(DeviceVK* device) : m_device(device) {}

QueryPoolVK::~QueryPoolVK() = default;

QueryType QueryPoolVK::GetType() const { return m_type; }

uint32_t QueryPoolVK::GetCount() const { return m_count; }

uint64_t QueryPoolVK::GetResult(uint32_t index) { (void)index; return 0; }

IQueryPool* CreateQueryPoolVK(DeviceVK* device, QueryType type, uint32_t count) {
    if (!device) return nullptr;
    auto* q = new QueryPoolVK(device);
    q->m_type = type;
    q->m_count = count;
    return q;
}

void DestroyQueryPoolVK(IQueryPool* pool) {
    if (pool) delete static_cast<QueryPoolVK*>(pool);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
