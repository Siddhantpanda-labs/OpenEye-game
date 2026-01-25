#include "Profiler.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <stack>
#include <unordered_map>

#include <glad/glad.h>

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
std::vector<std::pair<std::string, GLuint>> g_gpuStack;
bool g_gpuAvailable = true;

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

void Profiler::BeginGpuSample(const char* name) {
    if (!name || !g_gpuAvailable) return;
    if (!glGetString(GL_VERSION)) {
        g_gpuAvailable = false;
        return;
    }
    GLuint q = 0;
    glGenQueries(1, &q);
    if (!q) return;
    glBeginQuery(GL_TIME_ELAPSED, q);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteQueries(1, &q);
        g_gpuAvailable = false;
        return;
    }
    g_gpuStack.emplace_back(std::string("GPU:") + name, q);
}

void Profiler::EndGpuSample() {
    if (!g_gpuAvailable || g_gpuStack.empty()) return;
    auto entry = g_gpuStack.back();
    g_gpuStack.pop_back();
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 ns = 0;
    glGetQueryObjectui64v(entry.second, GL_QUERY_RESULT, &ns);
    glDeleteQueries(1, &entry.second);
    const double ms = static_cast<double>(ns) / 1'000'000.0;
    RecordSample(entry.first, ms);
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
    g_gpuStack.clear();
    g_lastFrameMs = 0.0;
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

