#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace api {
namespace debug {

struct ProfileSample {
    std::string name;
    double milliseconds = 0.0;
};

class Profiler {
public:
    static void BeginFrame();
    static void EndFrame();
    static void BeginSample(const std::string& name);
    static void EndSample();
    static void BeginGpuSample(const std::string& name);
    static void EndGpuSample();
    static double GetLastFrameMs();
    static std::vector<ProfileSample> GetLastFrameSamples();
    static double GetAverageTime(const std::string& name);
    static double GetMinTime(const std::string& name);
    static double GetMaxTime(const std::string& name);
    static size_t GetCallCount(const std::string& name);
    static void Reset();
    static bool ExportReport(const std::string& path);

    /** Per-frame counters (drawCalls, visibleMeshes, triangles, stateChanges, etc.). */
    static std::unordered_map<std::string, uint64_t> GetLastFrameCounters();
};

} // namespace debug
} // namespace api
} // namespace eyelib

