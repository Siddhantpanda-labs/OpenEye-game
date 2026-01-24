#pragma once

#include "Pose.h"
#include "Skeleton.h"
#include "eyelib/animation/IK.h"

#include <vector>

namespace eyelib {
namespace api {

struct IKChain {
    std::vector<int> bones;
    std::vector<float> lengths;
};

class IK {
public:
    static void SolveCCD(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                         const eyelib::math::Vec3& target, int iterations = 10);
    static void SolveFABRIK(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                            const eyelib::math::Vec3& target, int iterations = 10);
};

} // namespace api
} // namespace eyelib
