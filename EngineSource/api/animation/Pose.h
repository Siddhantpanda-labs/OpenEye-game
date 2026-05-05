#pragma once

#include "eyelib/animation/Pose.h"
#include "eyelib/math/Transform.h"

#include <memory>

namespace eyelib {
namespace api {

class Pose {
public:
    Pose();
    explicit Pose(size_t boneCount);
    ~Pose();

    Pose(const Pose&) = delete;
    Pose& operator=(const Pose&) = delete;

    size_t GetBoneCount() const;
    void SetBoneTransform(size_t index, const eyelib::math::Transform& t);
    const eyelib::math::Transform& GetBoneTransform(size_t index) const;

    std::shared_ptr<eyelib::animation::Pose> GetInternal() const;
    static std::shared_ptr<Pose> WrapInternal(const std::shared_ptr<eyelib::animation::Pose>& pose);

private:
    std::shared_ptr<eyelib::animation::Pose> m_pose;
};

} // namespace api
} // namespace eyelib
