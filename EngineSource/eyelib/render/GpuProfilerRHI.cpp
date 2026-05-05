#include "eyelib/render/GpuProfilerRHI.h"
#include "eyelib/debug/Profiler.h"

namespace eyelib {
namespace render {

GpuProfilerRHI::GpuProfilerRHI(rhi::IDevice* device) : m_device(device) {
    if (m_device) {
        m_pool = m_device->CreateQueryPool(rhi::QueryType::Timestamp, kPoolSize);
        m_cmd = m_device->GetImmediateCommandList();
    }
}

GpuProfilerRHI::~GpuProfilerRHI() {
    if (m_device && m_pool) {
        m_device->DestroyQueryPool(m_pool);
        m_pool = nullptr;
    }
    m_device = nullptr;
    m_cmd = nullptr;
}

void GpuProfilerRHI::BeginGpuSample(const char* name) {
    if (!m_device || !m_pool || !m_cmd || !name) return;
    if (m_stack.size() >= kPoolSize) return;

    const uint32_t index = m_nextIndex % kPoolSize;
    m_nextIndex++;
    m_stack.emplace_back(std::string("GPU:") + name, index);
    m_cmd->BeginQuery(m_pool, index);
}

void GpuProfilerRHI::EndGpuSample() {
    if (!m_device || !m_pool || !m_cmd || m_stack.empty()) return;

    const auto entry = m_stack.back();
    m_stack.pop_back();
    m_cmd->EndQuery(m_pool, entry.second);

    const uint64_t ns = m_pool->GetResult(entry.second);
    const double ms = static_cast<double>(ns) / 1'000'000.0;
    debug::Profiler::AddSample(entry.first.c_str(), ms);
}

} // namespace render
} // namespace eyelib
