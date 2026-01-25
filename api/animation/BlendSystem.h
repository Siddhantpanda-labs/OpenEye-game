#pragma once

#include "api/animation/AnimationClip.h"
#include "api/animation/Pose.h"
#include <vector>

namespace eyelib {
namespace api {

class BlendSystem {
public:
    static std::shared_ptr<Pose> SampleClip(const AnimationClip& clip, float time, size_t boneCount);
    static std::shared_ptr<Pose> Blend(const Pose& a, const Pose& b, float weight);
    static std::shared_ptr<Pose> BlendAdditive(const Pose& base, const Pose& additive, float weight);
    static std::shared_ptr<Pose> BlendClips(const AnimationClip& a, const AnimationClip& b,
                                            float timeA, float timeB, size_t boneCount, float weight);
    static std::shared_ptr<Pose> Blend1D(const std::vector<std::shared_ptr<Pose>>& poses,
                                         const std::vector<float>& weights);
    static std::shared_ptr<Pose> Blend2D(const std::vector<std::shared_ptr<Pose>>& poses,
                                         const std::vector<float>& weights);
};

} // namespace api
} // namespace eyelib
