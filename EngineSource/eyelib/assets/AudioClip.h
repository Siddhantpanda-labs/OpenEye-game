#pragma once

#include <vector>

namespace eyelib {
namespace assets {

struct AudioClip {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples;
};

} // namespace assets
} // namespace eyelib
