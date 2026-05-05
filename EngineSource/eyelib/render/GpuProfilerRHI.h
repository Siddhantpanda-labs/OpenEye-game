#pragma once

#include "eyelib/debug/Profiler.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/** RHI-based GPU profiler; register with Profiler::SetGpuProfiler() when device is available. */
class GpuProfilerRHI : public debug::IGpuProfiler {
public:
    explicit GpuProfilerRHI(rhi::IDevice* device);
    ~GpuProfilerRHI() override;

    void BeginGpuSample(const char* name) override;
    void EndGpuSample() override;

private:
    static constexpr uint32_t kPoolSize = 32u;

    rhi::IDevice* m_device = nullptr;
    rhi::IQueryPool* m_pool = nullptr;
    rhi::ICommandList* m_cmd = nullptr;
    std::vector<std::pair<std::string, uint32_t>> m_stack;
    uint32_t m_nextIndex = 0;
};

} // namespace render
} // namespace eyelib
