#include "eyelib/rendering/debug/RenderStats.h"
#include "eyelib/rendering/core/GpuProfiler.h"
#include <sstream>

namespace eyelib {
namespace rendering {

void RenderStats::CaptureFromProfiler(uint64_t frameIndex, const GpuProfiler& profiler) {
    m_frameIndex = frameIndex;
    m_pipelineCounters.clear();

    std::vector<GpuProfiler::NamedPipelineStat> named;
    profiler.GetLastPipelineStatsNamed(named);
    m_pipelineCounters.reserve(named.size());
    for (const auto& c : named) {
        m_pipelineCounters.push_back({c.name ? c.name : "", c.value});
    }
}

std::string RenderStats::FormatPipelineSummary() const {
    std::ostringstream oss;
    oss << "gpu_stats[f=" << m_frameIndex << "]";
    if (m_pipelineCounters.empty()) {
        oss << " (no counters)";
        return oss.str();
    }
    for (const auto& c : m_pipelineCounters) {
        oss << ' ' << c.name << '=' << c.value;
    }
    return oss.str();
}
}  // namespace rendering
}  // namespace eyelib
