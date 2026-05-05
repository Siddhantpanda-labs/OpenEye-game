#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

class GpuProfiler;

class RenderStats {
public:
    struct Counter {
        std::string name;
        uint64_t value = 0;
    };

    RenderStats() = default;

    void CaptureFromProfiler(uint64_t frameIndex, const GpuProfiler& profiler);
    /** Human-readable one-line summary for logs/overlays (e.g. "gpu_stats[f=12] ia_vertices=..."). */
    std::string FormatPipelineSummary() const;

    uint64_t GetFrameIndex() const { return m_frameIndex; }
    const std::vector<Counter>& GetPipelineCounters() const { return m_pipelineCounters; }

private:
    uint64_t m_frameIndex = 0;
    std::vector<Counter> m_pipelineCounters;
};
}  // namespace rendering
}  // namespace eyelib
