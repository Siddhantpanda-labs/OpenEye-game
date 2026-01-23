#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace debug {

struct ProfileSample {
    std::string name;
    double milliseconds = 0.0;
};

class Profiler {
public:
    static void BeginFrame();
    static void EndFrame();

    static void AddSample(const char* name, double milliseconds);

    static double GetLastFrameMs();
    static std::vector<ProfileSample> GetLastFrameSamples();
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

