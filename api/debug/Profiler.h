#pragma once

#include <string>
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
    static double GetLastFrameMs();
    static std::vector<ProfileSample> GetLastFrameSamples();
};

} // namespace debug
} // namespace api
} // namespace eyelib

