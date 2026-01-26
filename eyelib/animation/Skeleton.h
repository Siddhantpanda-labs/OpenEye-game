#pragma once

#include "eyelib/math/Types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace animation {

struct Bone {
    std::string name;
    int parentIndex = -1;

    math::Vec3 bindPosition{0.0f, 0.0f, 0.0f};
    math::Quat bindRotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vec3 bindScale{1.0f, 1.0f, 1.0f};

    math::Mat4 inverseBindMatrix{1.0f};
};

class Skeleton {
public:
    void AddBone(const Bone& bone);
    void ModifyBone(size_t index, const Bone& bone);
    void RemoveBone(size_t index);
    void Clear();

    size_t BoneCount() const { return m_bones.size(); }
    const Bone& GetBone(size_t index) const { return m_bones[index]; }
    Bone& GetBone(size_t index) { return m_bones[index]; }

    int FindBoneIndex(const std::string& name) const;
    void BuildBoneMap();

    bool IsValid() const;

    void SetGlobalInverseTransform(const math::Mat4& m) { m_globalInverse = m; }
    const math::Mat4& GetGlobalInverseTransform() const { return m_globalInverse; }

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int> m_boneNameToIndex;
    bool m_boneMapBuilt = false;
    math::Mat4 m_globalInverse{1.0f};
};

} // namespace animation
} // namespace eyelib

