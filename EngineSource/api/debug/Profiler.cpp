#include "Profiler.h"

#include "eyelib/debug/Profiler.h"

namespace eyelib {
namespace api {
namespace debug {

namespace {
static ProfileSample FromInternal(const eyelib::debug::ProfileSample& s) {
    ProfileSample out;
    out.name = s.name;
    out.milliseconds = s.milliseconds;
    return out;
}
} // namespace

void Profiler::BeginFrame() { eyelib::debug::Profiler::BeginFrame(); }
void Profiler::EndFrame() { eyelib::debug::Profiler::EndFrame(); }
void Profiler::BeginSample(const std::string& name) { eyelib::debug::Profiler::BeginSample(name.c_str()); }
void Profiler::EndSample() { eyelib::debug::Profiler::EndSample(); }
void Profiler::BeginGpuSample(const std::string& name) { eyelib::debug::Profiler::BeginGpuSample(name.c_str()); }
void Profiler::EndGpuSample() { eyelib::debug::Profiler::EndGpuSample(); }
double Profiler::GetLastFrameMs() { return eyelib::debug::Profiler::GetLastFrameMs(); }

std::vector<ProfileSample> Profiler::GetLastFrameSamples() {
    auto samples = eyelib::debug::Profiler::GetLastFrameSamples();
    std::vector<ProfileSample> out;
    out.reserve(samples.size());
    for (const auto& s : samples) out.push_back(FromInternal(s));
    return out;
}

double Profiler::GetAverageTime(const std::string& name) { return eyelib::debug::Profiler::GetAverageTime(name); }
double Profiler::GetMinTime(const std::string& name) { return eyelib::debug::Profiler::GetMinTime(name); }
double Profiler::GetMaxTime(const std::string& name) { return eyelib::debug::Profiler::GetMaxTime(name); }
size_t Profiler::GetCallCount(const std::string& name) { return eyelib::debug::Profiler::GetCallCount(name); }
void Profiler::Reset() { eyelib::debug::Profiler::Reset(); }
bool Profiler::ExportReport(const std::string& path) { return eyelib::debug::Profiler::ExportReport(path); }

} // namespace debug
} // namespace api
} // namespace eyelib

