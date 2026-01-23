#include "Profiler.h"

#include <chrono>
#include <mutex>

namespace eyelib {
namespace debug {

namespace {
using Clock = std::chrono::steady_clock;

std::mutex g_profMutex;
Clock::time_point g_frameStart{};
double g_lastFrameMs = 0.0;
std::vector<ProfileSample> g_lastSamples;
std::vector<ProfileSample> g_currentSamples;

static uint64_t NowTicks() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

static double TicksToMs(uint64_t dtTicks) {
    return static_cast<double>(dtTicks) / 1'000'000.0;
}
} // namespace

void Profiler::BeginFrame() {
    std::scoped_lock lock(g_profMutex);
    g_frameStart = Clock::now();
    g_currentSamples.clear();
}

void Profiler::EndFrame() {
    std::scoped_lock lock(g_profMutex);
    const auto end = Clock::now();
    g_lastFrameMs = std::chrono::duration<double, std::milli>(end - g_frameStart).count();
    g_lastSamples = g_currentSamples;
}

void Profiler::AddSample(const char* name, double milliseconds) {
    if (!name) return;
    std::scoped_lock lock(g_profMutex);
    g_currentSamples.push_back(ProfileSample{ std::string(name), milliseconds });
}

double Profiler::GetLastFrameMs() {
    std::scoped_lock lock(g_profMutex);
    return g_lastFrameMs;
}

std::vector<ProfileSample> Profiler::GetLastFrameSamples() {
    std::scoped_lock lock(g_profMutex);
    return g_lastSamples;
}

ScopeTimer::ScopeTimer(const char* name) : m_name(name), m_startTicks(NowTicks()) {}

ScopeTimer::~ScopeTimer() {
    const uint64_t end = NowTicks();
    const double ms = TicksToMs(end - m_startTicks);
    Profiler::AddSample(m_name ? m_name : "scope", ms);
}

} // namespace debug
} // namespace eyelib

