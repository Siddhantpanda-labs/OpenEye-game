#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <string>

namespace eyelib {
namespace animation { class Skeleton; }
namespace api {

class Skeleton {
public:
    struct Bone {
        std::string name;
        int parentIndex = -1;
        eyelib::math::Vec3 bindPosition{0.0f};
        eyelib::math::Quat bindRotation{1.0f, 0.0f, 0.0f, 0.0f};
        eyelib::math::Vec3 bindScale{1.0f};
        eyelib::math::Mat4 inverseBindMatrix{1.0f};
    };

    Skeleton();
    ~Skeleton();

    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;

    void AddBone(const Bone& bone);
    void ModifyBone(size_t index, const Bone& bone);
    void RemoveBone(size_t index);
    Bone GetBone(size_t index) const;
    void BuildBoneMap();
    size_t BoneCount() const;
    int FindBoneIndex(const std::string& name) const;

    static std::shared_ptr<Skeleton> WrapInternal(const std::shared_ptr<eyelib::animation::Skeleton>& skeleton);
    std::shared_ptr<eyelib::animation::Skeleton> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Animator;
};

} // namespace api
} // namespace eyelib
