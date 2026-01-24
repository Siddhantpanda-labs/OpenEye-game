#pragma once

#include "Pose.h"
#include "Skeleton.h"
#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace animation {

struct IKConstraint {
    float minAngle = -3.14159f;
    float maxAngle = 3.14159f;
};

struct IKChain {
    std::vector<int> bones;      // ordered root -> end effector
    std::vector<float> lengths;  // per-bone length
    std::vector<IKConstraint> constraints;
};

class IK {
public:
    static void SolveCCD(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                         const math::Vec3& target, int iterations = 10);
    static void SolveFABRIK(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                            const math::Vec3& target, int iterations = 10);
};

} // namespace animation
} // namespace eyelib
