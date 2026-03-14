#include "Profiler.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <stack>
#include <unordered_map>

namespace eyelib {
namespace debug {

namespace {
using Clock = std::chrono::steady_clock;

std::mutex g_profMutex;
Clock::time_point g_frameStart{};
double g_lastFrameMs = 0.0;
std::vector<ProfileSample> g_lastSamples;
std::vector<ProfileSample> g_currentSamples;
std::unordered_map<std::string, ProfileStats> g_stats;
std::vector<std::pair<std::string, uint64_t>> g_stack;
IGpuProfiler* g_gpuProfiler = nullptr;
std::unordered_map<std::string, uint64_t> g_frameCounters;
std::unordered_map<std::string, uint64_t> g_lastFrameCounters;

static uint64_t NowTicks() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

static double TicksToMs(uint64_t dtTicks) {
    return static_cast<double>(dtTicks) / 1'000'000.0;
}

static void RecordSample(const std::string& name, double milliseconds) {
    g_currentSamples.push_back(ProfileSample{ name, milliseconds });
    auto& stats = g_stats[name];
    stats.callCount += 1;
    if (stats.callCount == 1) {
        stats.minMs = milliseconds;
        stats.maxMs = milliseconds;
        stats.averageMs = milliseconds;
    } else {
        stats.minMs = std::min(stats.minMs, milliseconds);
        stats.maxMs = std::max(stats.maxMs, milliseconds);
        const double total = stats.averageMs * static_cast<double>(stats.callCount - 1) + milliseconds;
        stats.averageMs = total / static_cast<double>(stats.callCount);
    }
}
} // namespace

void Profiler::BeginFrame() {
    std::scoped_lock lock(g_profMutex);
    g_frameStart = Clock::now();
    g_currentSamples.clear();
    g_stack.clear();
    g_frameCounters.clear();
}

void Profiler::EndFrame() {
    std::scoped_lock lock(g_profMutex);
    const auto end = Clock::now();
    g_lastFrameMs = std::chrono::duration<double, std::milli>(end - g_frameStart).count();
    g_lastSamples = g_currentSamples;
    g_lastFrameCounters = g_frameCounters;
}

void Profiler::AddSample(const char* name, double milliseconds) {
    if (!name) return;
    std::scoped_lock lock(g_profMutex);
    RecordSample(std::string(name), milliseconds);
}

void Profiler::BeginSample(const char* name) {
    if (!name) return;
    std::scoped_lock lock(g_profMutex);
    g_stack.emplace_back(std::string(name), NowTicks());
}

void Profiler::EndSample() {
    std::scoped_lock lock(g_profMutex);
    if (g_stack.empty()) return;
    const auto entry = g_stack.back();
    g_stack.pop_back();
    const uint64_t end = NowTicks();
    const double ms = TicksToMs(end - entry.second);
    RecordSample(entry.first, ms);
}

void Profiler::SetGpuProfiler(IGpuProfiler* profiler) {
    std::scoped_lock lock(g_profMutex);
    g_gpuProfiler = profiler;
}

void Profiler::BeginGpuSample(const char* name) {
    if (g_gpuProfiler && name)
        g_gpuProfiler->BeginGpuSample(name);
}

void Profiler::EndGpuSample() {
    if (g_gpuProfiler)
        g_gpuProfiler->EndGpuSample();
}

double Profiler::GetLastFrameMs() {
    std::scoped_lock lock(g_profMutex);
    return g_lastFrameMs;
}

std::vector<ProfileSample> Profiler::GetLastFrameSamples() {
    std::scoped_lock lock(g_profMutex);
    return g_lastSamples;
}

double Profiler::GetAverageTime(const std::string& name) {
    std::scoped_lock lock(g_profMutex);
    auto it = g_stats.find(name);
    return it == g_stats.end() ? 0.0 : it->second.averageMs;
}

double Profiler::GetMinTime(const std::string& name) {
    std::scoped_lock lock(g_profMutex);
    auto it = g_stats.find(name);
    return it == g_stats.end() ? 0.0 : it->second.minMs;
}

double Profiler::GetMaxTime(const std::string& name) {
    std::scoped_lock lock(g_profMutex);
    auto it = g_stats.find(name);
    return it == g_stats.end() ? 0.0 : it->second.maxMs;
}

size_t Profiler::GetCallCount(const std::string& name) {
    std::scoped_lock lock(g_profMutex);
    auto it = g_stats.find(name);
    return it == g_stats.end() ? 0u : it->second.callCount;
}

void Profiler::Reset() {
    std::scoped_lock lock(g_profMutex);
    g_stats.clear();
    g_currentSamples.clear();
    g_lastSamples.clear();
    g_stack.clear();
    g_lastFrameMs = 0.0;
    g_frameCounters.clear();
    g_lastFrameCounters.clear();
}

void Profiler::SetFrameCounter(const char* name, uint64_t value) {
    if (!name) return;
    std::scoped_lock lock(g_profMutex);
    g_frameCounters[std::string(name)] = value;
}

void Profiler::AddFrameCounter(const char* name, uint64_t delta) {
    if (!name) return;
    std::scoped_lock lock(g_profMutex);
    g_frameCounters[std::string(name)] += delta;
}

uint64_t Profiler::GetFrameCounter(const char* name) {
    if (!name) return 0;
    std::scoped_lock lock(g_profMutex);
    auto it = g_frameCounters.find(std::string(name));
    return it == g_frameCounters.end() ? 0u : it->second;
}

std::unordered_map<std::string, uint64_t> Profiler::GetLastFrameCounters() {
    std::scoped_lock lock(g_profMutex);
    return g_lastFrameCounters;
}

bool Profiler::ExportReport(const std::string& path) {
    std::scoped_lock lock(g_profMutex);
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << "Profiler Report\n";
    out << "Last Frame: " << g_lastFrameMs << " ms\n\n";
    out << "Samples:\n";
    for (const auto& entry : g_stats) {
        const auto& s = entry.second;
        out << entry.first << ": avg=" << s.averageMs
            << " min=" << s.minMs
            << " max=" << s.maxMs
            << " calls=" << s.callCount << "\n";
    }
    return true;
}

ScopeTimer::ScopeTimer(const char* name) : m_name(name), m_startTicks(NowTicks()) {}

ScopeTimer::~ScopeTimer() {
    const uint64_t end = NowTicks();
    const double ms = TicksToMs(end - m_startTicks);
    Profiler::AddSample(m_name ? m_name : "scope", ms);
}

} // namespace debug
} // namespace eyelib

