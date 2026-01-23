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
double Profiler::GetLastFrameMs() { return eyelib::debug::Profiler::GetLastFrameMs(); }

std::vector<ProfileSample> Profiler::GetLastFrameSamples() {
    auto samples = eyelib::debug::Profiler::GetLastFrameSamples();
    std::vector<ProfileSample> out;
    out.reserve(samples.size());
    for (const auto& s : samples) out.push_back(FromInternal(s));
    return out;
}

} // namespace debug
} // namespace api
} // namespace eyelib

