#include "IK.h"

namespace eyelib {
namespace api {

namespace {
eyelib::animation::IKChain Convert(const IKChain& chain) {
    eyelib::animation::IKChain c;
    c.bones = chain.bones;
    c.lengths = chain.lengths;
    c.constraints.resize(chain.bones.size());
    return c;
}
} // namespace

void IK::SolveCCD(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                  const eyelib::math::Vec3& target, int iterations) {
    eyelib::animation::IK::SolveCCD(*skeleton.GetInternal(), *pose.GetInternal(), Convert(chain), target, iterations);
}

void IK::SolveFABRIK(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                     const eyelib::math::Vec3& target, int iterations) {
    eyelib::animation::IK::SolveFABRIK(*skeleton.GetInternal(), *pose.GetInternal(), Convert(chain), target, iterations);
}

} // namespace api
} // namespace eyelib
