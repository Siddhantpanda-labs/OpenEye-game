#include "Pose.h"

namespace eyelib {
namespace api {

Pose::Pose() : m_pose(std::make_shared<eyelib::animation::Pose>()) {}
Pose::Pose(size_t boneCount) : m_pose(std::make_shared<eyelib::animation::Pose>(boneCount)) {}
Pose::~Pose() = default;

size_t Pose::GetBoneCount() const {
    return m_pose ? m_pose->GetBoneCount() : 0;
}

void Pose::SetBoneTransform(size_t index, const eyelib::math::Transform& t) {
    if (m_pose) m_pose->SetBoneTransform(index, t);
}

const eyelib::math::Transform& Pose::GetBoneTransform(size_t index) const {
    return m_pose->GetBoneTransform(index);
}

std::shared_ptr<eyelib::animation::Pose> Pose::GetInternal() const {
    return m_pose;
}

} // namespace api
} // namespace eyelib
