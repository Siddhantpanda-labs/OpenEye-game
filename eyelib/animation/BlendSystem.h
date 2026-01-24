#pragma once

#include "AnimationClip.h"
#include "Pose.h"

#include <vector>

namespace eyelib {
namespace animation {

class BlendSystem {
public:
    static Pose SampleClip(const AnimationClip& clip, float time, size_t boneCount);
    static Pose Blend(const Pose& a, const Pose& b, float weight);
    static Pose BlendAdditive(const Pose& base, const Pose& additive, float weight);
    static Pose BlendClips(const AnimationClip& a, const AnimationClip& b,
                           float timeA, float timeB, size_t boneCount, float weight);
    static Pose Blend1D(const std::vector<Pose>& poses, const std::vector<float>& weights);
    static Pose Blend2D(const std::vector<Pose>& poses, const std::vector<float>& weights);
};

} // namespace animation
} // namespace eyelib
