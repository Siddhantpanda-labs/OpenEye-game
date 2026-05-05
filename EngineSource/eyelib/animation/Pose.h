#pragma once

#include "eyelib/math/Transform.h"
#include <vector>

namespace eyelib {
namespace animation {

class Pose {
public:
    Pose() = default;
    explicit Pose(size_t boneCount) : m_bones(boneCount) {}

    size_t GetBoneCount() const { return m_bones.size(); }
    void Resize(size_t boneCount) { m_bones.resize(boneCount); }

    void SetBoneTransform(size_t index, const math::Transform& t);
    const math::Transform& GetBoneTransform(size_t index) const;
    math::Transform& GetBoneTransform(size_t index);

    Pose Blend(const Pose& other, float weight) const;
    void BlendInPlace(const Pose& other, float weight);
    Pose ApplyAdditive(const Pose& additive, float weight) const;

private:
    std::vector<math::Transform> m_bones;
};

} // namespace animation
} // namespace eyelib
