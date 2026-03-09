#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace debug {

/** Implemented by render module (e.g. GpuProfilerRHI) so Profiler stays backend-agnostic. */
class IGpuProfiler {
public:
    virtual ~IGpuProfiler() = default;
    virtual void BeginGpuSample(const char* name) = 0;
    virtual void EndGpuSample() = 0;
};

struct ProfileSample {
    std::string name;
    double milliseconds = 0.0;
};

struct ProfileStats {
    double averageMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    size_t callCount = 0;
};

class Profiler {
public:
    static void BeginFrame();
    static void EndFrame();

    static void AddSample(const char* name, double milliseconds);
    static void BeginSample(const char* name);
    static void EndSample();
    static void SetGpuProfiler(IGpuProfiler* profiler);
    static void BeginGpuSample(const char* name);
    static void EndGpuSample();

    static double GetLastFrameMs();
    static std::vector<ProfileSample> GetLastFrameSamples();

    static double GetAverageTime(const std::string& name);
    static double GetMinTime(const std::string& name);
    static double GetMaxTime(const std::string& name);
    static size_t GetCallCount(const std::string& name);
    static void Reset();
    static bool ExportReport(const std::string& path);
};

class ScopeTimer {
public:
    explicit ScopeTimer(const char* name);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    const char* m_name = nullptr;
    uint64_t m_startTicks = 0;
};

} // namespace debug
} // namespace eyelib

