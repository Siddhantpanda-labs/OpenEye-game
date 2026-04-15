#include "eyelib/rendering/core/GpuProfiler.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/backend/RenderBackendPolicy.h"
#include <array>

namespace eyelib {
namespace rendering {

namespace {

// Slot names matching kDefaultPipelineStatisticsFlags (IA_VERTICES, IA_PRIMITIVES, VS_INVOCATIONS, FS_INVOCATIONS).
constexpr std::array<const char*, 4> kDefaultPipelineStatNames = {
    "ia_vertices",
    "ia_primitives",
    "vs_invocations",
    "fs_invocations",
};

} // namespace

GpuProfiler::~GpuProfiler() = default;

void GpuProfiler::BeginFrame(rhi::IDevice* device, rhi::ICommandList* commandList) {
    m_queryOpen = false;
    if (!device || !commandList) return;
    if (!backend::SupportsPipelineStatisticsQueries(device)) return;

    if (!m_pipelineStatsPool) {
        m_pipelineStatsPool = device->CreateQueryPool(rhi::QueryType::PipelineStatistics, kPoolCount, 0u);
        if (!m_pipelineStatsPool) return;
        m_enabled = true;
    }

    if (!m_enabled || !m_pipelineStatsPool) return;

    if (m_haveReadableResult) {
        const uint32_t readIndex = (m_writeIndex + kPoolCount - 1u) % kPoolCount;
        const uint32_t slotCount = m_pipelineStatsPool->GetPipelineStatisticsSlotCount();
        if (slotCount > 0) {
            m_lastPipelineStats.assign(slotCount, 0u);
            const uint32_t written =
                m_pipelineStatsPool->GetPipelineStatistics(readIndex, m_lastPipelineStats.data(), slotCount);
            if (written != slotCount) {
                m_lastPipelineStats.clear();
            }
        } else {
            m_lastPipelineStats.clear();
        }
    }

    commandList->ResetQueryPool(m_pipelineStatsPool, m_writeIndex, 1u);
    commandList->BeginQuery(m_pipelineStatsPool, m_writeIndex);
    m_queryOpen = true;
}

void GpuProfiler::EndFrame(rhi::ICommandList* commandList) {
    if (!commandList || !m_pipelineStatsPool || !m_queryOpen) return;
    commandList->EndQuery(m_pipelineStatsPool, m_writeIndex);
    m_writeIndex = (m_writeIndex + 1u) % kPoolCount;
    m_haveReadableResult = true;
    m_queryOpen = false;
}

void GpuProfiler::Shutdown(rhi::IDevice* device) {
    m_queryOpen = false;
    m_haveReadableResult = false;
    m_enabled = false;
    m_writeIndex = 0;
    m_lastPipelineStats.clear();
    if (m_pipelineStatsPool && device) {
        device->DestroyQueryPool(m_pipelineStatsPool);
    }
    m_pipelineStatsPool = nullptr;
}

void GpuProfiler::GetLastPipelineStatsNamed(std::vector<NamedPipelineStat>& out) const {
    out.clear();
    if (m_lastPipelineStats.empty()) return;
    out.reserve(m_lastPipelineStats.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_lastPipelineStats.size()); ++i) {
        const char* name = (i < kDefaultPipelineStatNames.size()) ? kDefaultPipelineStatNames[i] : "pipeline_stat";
        out.push_back({name, m_lastPipelineStats[i]});
    }
}
}  // namespace rendering
}  // namespace eyelib
