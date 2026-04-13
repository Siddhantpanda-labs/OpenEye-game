#include "eyelib/rendering/rhi/Vulkan/sync/QueryPoolVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

QueryPoolVK::QueryPoolVK(DeviceVK* device) : m_device(device) {}

QueryPoolVK::~QueryPoolVK() {
    if (m_queryPool && m_device) {
        VkDevice d = static_cast<VkDevice>(m_device->GetDevice());
        if (d)
            vkDestroyQueryPool(d, static_cast<VkQueryPool>(m_queryPool), nullptr);
    }
    m_queryPool = nullptr;
}

QueryType QueryPoolVK::GetType() const { return m_type; }

uint32_t QueryPoolVK::GetCount() const { return m_count; }

uint64_t QueryPoolVK::GetResult(uint32_t index) {
    if (!m_device || !m_queryPool || index >= m_count) return 0;
    VkDevice d = static_cast<VkDevice>(m_device->GetDevice());
    if (!d) return 0;
    VkQueryPool pool = static_cast<VkQueryPool>(m_queryPool);
    uint64_t raw = 0;
    VkResult r = vkGetQueryPoolResults(d, pool, index, 1, sizeof(raw), &raw, sizeof(raw),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (r != VK_SUCCESS) {
        core::Log::WarnF("QueryPoolVK::GetResult: vkGetQueryPoolResults failed (0x%x) index=%u", r, index);
        return 0;
    }
    if (m_type == QueryType::Timestamp) {
        const double ns = static_cast<double>(raw) * m_timestampPeriodNs;
        return static_cast<uint64_t>(ns + 0.5);
    }
    return raw;
}

IQueryPool* CreateQueryPoolVK(DeviceVK* device, QueryType type, uint32_t count) {
    if (!device || count == 0) return nullptr;
    VkDevice vkdev = static_cast<VkDevice>(device->GetDevice());
    if (!vkdev) return nullptr;

    VkQueryPoolCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    ci.queryCount = count;
    double timestampPeriodNs = 1.0;

    if (type == QueryType::Timestamp) {
        if (!device->GetCapabilities().supportsGpuTimestampQueries) {
            core::Log::Error("CreateQueryPoolVK: GPU timestamp queries not supported (RHICapabilities::supportsGpuTimestampQueries).");
            return nullptr;
        }
        VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(device->GetPhysicalDevice());
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(phys, &props);
        if (props.limits.timestampPeriod <= 0.f) {
            core::Log::Error("CreateQueryPoolVK: timestamp queries not supported (timestampPeriod == 0).");
            return nullptr;
        }
        timestampPeriodNs = static_cast<double>(props.limits.timestampPeriod);
        ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    } else if (type == QueryType::Occlusion) {
        if (!device->GetCapabilities().supportsOcclusionQueries) {
            core::Log::Error("CreateQueryPoolVK: occlusion queries not supported (RHICapabilities::supportsOcclusionQueries).");
            return nullptr;
        }
        ci.queryType = VK_QUERY_TYPE_OCCLUSION;
    } else {
        core::Log::Error("CreateQueryPoolVK: unsupported QueryType for Vulkan.");
        return nullptr;
    }

    VkQueryPool pool = VK_NULL_HANDLE;
    if (vkCreateQueryPool(vkdev, &ci, nullptr, &pool) != VK_SUCCESS) {
        core::Log::Error("CreateQueryPoolVK: vkCreateQueryPool failed.");
        return nullptr;
    }
    auto* q = new QueryPoolVK(device);
    q->m_type = type;
    q->m_count = count;
    q->m_queryPool = pool;
    q->m_timestampPeriodNs = timestampPeriodNs;
    return q;
}

void DestroyQueryPoolVK(IQueryPool* pool) {
    if (pool) delete static_cast<QueryPoolVK*>(pool);
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
