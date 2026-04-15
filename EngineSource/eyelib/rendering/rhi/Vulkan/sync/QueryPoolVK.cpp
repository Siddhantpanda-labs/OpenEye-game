#include "eyelib/rendering/rhi/Vulkan/sync/QueryPoolVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/backend/RenderBackendPolicy.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <bit>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

/** Core `VkQueryPipelineStatisticFlagBits` through compute (no mesh/task extension bits). */
constexpr uint32_t kCorePipelineStatisticsMask = 0x7FFu;

} // namespace

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
    if (m_type == QueryType::PipelineStatistics) {
        core::Log::Warn("QueryPoolVK::GetResult: use GetPipelineStatistics() for QueryType::PipelineStatistics.");
        return 0;
    }
    if (!m_device || !m_queryPool) {
        core::Log::Warn("QueryPoolVK::GetResult: query pool is not initialized.");
        return 0;
    }
    if (index >= m_count) {
        core::Log::WarnF("QueryPoolVK::GetResult: index out of range (index=%u count=%u).", index, m_count);
        return 0;
    }
    VkDevice d = static_cast<VkDevice>(m_device->GetDevice());
    if (!d) {
        core::Log::Warn("QueryPoolVK::GetResult: VkDevice is null.");
        return 0;
    }
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

uint32_t QueryPoolVK::GetPipelineStatisticsSlotCount() const {
    return m_type == QueryType::PipelineStatistics ? m_pipelineStatisticsSlotCount : 0u;
}

uint32_t QueryPoolVK::GetPipelineStatistics(uint32_t queryIndex, uint64_t* outSlots, uint32_t maxSlots) {
    if (m_type != QueryType::PipelineStatistics) {
        core::Log::Warn("QueryPoolVK::GetPipelineStatistics: query pool type is not PipelineStatistics.");
        return 0;
    }
    if (!m_device || !m_queryPool) {
        core::Log::Warn("QueryPoolVK::GetPipelineStatistics: query pool is not initialized.");
        return 0;
    }
    if (!outSlots || maxSlots == 0) {
        core::Log::Warn("QueryPoolVK::GetPipelineStatistics: output buffer is null or maxSlots is zero.");
        return 0;
    }
    if (queryIndex >= m_count) {
        core::Log::WarnF("QueryPoolVK::GetPipelineStatistics: index out of range (index=%u count=%u).", queryIndex, m_count);
        return 0;
    }
    if (m_pipelineStatisticsSlotCount == 0)
        return 0;
    const uint32_t n = m_pipelineStatisticsSlotCount;
    if (maxSlots < n) {
        core::Log::WarnF("QueryPoolVK::GetPipelineStatistics: maxSlots (%u) < GetPipelineStatisticsSlotCount() (%u)", maxSlots, n);
        return 0;
    }
    VkDevice d = static_cast<VkDevice>(m_device->GetDevice());
    if (!d) return 0;
    VkQueryPool pool = static_cast<VkQueryPool>(m_queryPool);
    const VkDeviceSize stride = static_cast<VkDeviceSize>(n) * sizeof(uint64_t);
    const size_t dataBytes = static_cast<size_t>(n) * sizeof(uint64_t);
    VkResult r = vkGetQueryPoolResults(d, pool, queryIndex, 1, dataBytes, outSlots, stride,
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (r != VK_SUCCESS) {
        core::Log::WarnF("QueryPoolVK::GetPipelineStatistics: vkGetQueryPoolResults failed (0x%x) index=%u", r, queryIndex);
        return 0;
    }
    return n;
}

IQueryPool* CreateQueryPoolVK(DeviceVK* device, QueryType type, uint32_t count, PipelineStatisticsFlags pipelineStatisticsMask) {
    if (!device || count == 0) return nullptr;
    VkDevice vkdev = static_cast<VkDevice>(device->GetDevice());
    if (!vkdev) return nullptr;

    VkQueryPoolCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    ci.queryCount = count;
    double timestampPeriodNs = 1.0;
    uint32_t statsFlagsU32 = 0;
    uint32_t statsSlotCount = 0;

    if (type == QueryType::Timestamp) {
        if (!::eyelib::rendering::backend::SupportsGpuTimestampQueries(device)) {
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
        if (!::eyelib::rendering::backend::SupportsOcclusionQueries(device)) {
            core::Log::Error("CreateQueryPoolVK: occlusion queries not supported (RHICapabilities::supportsOcclusionQueries).");
            return nullptr;
        }
        ci.queryType = VK_QUERY_TYPE_OCCLUSION;
    } else if (type == QueryType::PipelineStatistics) {
        if (!::eyelib::rendering::backend::SupportsPipelineStatisticsQueries(device)) {
            core::Log::Error(
                "CreateQueryPoolVK: pipeline statistics queries not supported (RHICapabilities::supportsPipelineStatisticsQueries).");
            return nullptr;
        }
        PipelineStatisticsFlags mask = pipelineStatisticsMask ? pipelineStatisticsMask : kDefaultPipelineStatisticsFlags;
        mask &= kCorePipelineStatisticsMask;
        if (mask == 0) {
            core::Log::Error("CreateQueryPoolVK: pipeline statistics mask is empty after sanitization.");
            return nullptr;
        }
        statsFlagsU32 = mask;
        statsSlotCount = static_cast<uint32_t>(std::popcount(mask));
        ci.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        ci.pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(mask);
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
    q->m_pipelineStatisticsFlags = statsFlagsU32;
    q->m_pipelineStatisticsSlotCount = statsSlotCount;
    return q;
}

void DestroyQueryPoolVK(IQueryPool* pool) {
    if (pool) delete static_cast<QueryPoolVK*>(pool);
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
