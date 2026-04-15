#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {

namespace rhi {
struct IDevice;
struct ICommandList;
struct IQueryPool;
}

class GpuProfiler {
public:
    struct NamedPipelineStat {
        const char* name = "";
        uint64_t value = 0;
    };

    GpuProfiler() = default;
    ~GpuProfiler();

    void BeginFrame(rhi::IDevice* device, rhi::ICommandList* commandList);
    void EndFrame(rhi::ICommandList* commandList);
    void Shutdown(rhi::IDevice* device);

    bool IsEnabled() const { return m_enabled; }
    uint32_t LastPipelineStatsSlotCount() const { return static_cast<uint32_t>(m_lastPipelineStats.size()); }
    const std::vector<uint64_t>& LastPipelineStats() const { return m_lastPipelineStats; }
    void GetLastPipelineStatsNamed(std::vector<NamedPipelineStat>& out) const;

private:
    static constexpr uint32_t kPoolCount = 2;
    rhi::IQueryPool* m_pipelineStatsPool = nullptr;
    uint32_t m_writeIndex = 0;
    bool m_haveReadableResult = false;
    bool m_enabled = false;
    bool m_queryOpen = false;
    std::vector<uint64_t> m_lastPipelineStats;
};

}  // namespace rendering
}  // namespace eyelib
