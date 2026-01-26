#include "Skeleton.h"

#include "eyelib/animation/Skeleton.h"

namespace eyelib {
namespace api {

class Skeleton::Impl {
public:
    std::shared_ptr<eyelib::animation::Skeleton> skeleton = std::make_shared<eyelib::animation::Skeleton>();
};

Skeleton::Skeleton() : m_impl(std::make_unique<Impl>()) {}
Skeleton::~Skeleton() = default;

void Skeleton::AddBone(const Bone& bone) {
    eyelib::animation::Bone b;
    b.name = bone.name;
    b.parentIndex = bone.parentIndex;
    b.bindPosition = bone.bindPosition;
    b.bindRotation = bone.bindRotation;
    b.bindScale = bone.bindScale;
    b.inverseBindMatrix = bone.inverseBindMatrix;
    m_impl->skeleton->AddBone(b);
}

void Skeleton::ModifyBone(size_t index, const Bone& bone) {
    if (index >= m_impl->skeleton->BoneCount()) return;
    
    eyelib::animation::Bone b;
    b.name = bone.name;
    b.parentIndex = bone.parentIndex;
    b.bindPosition = bone.bindPosition;
    b.bindRotation = bone.bindRotation;
    b.bindScale = bone.bindScale;
    b.inverseBindMatrix = bone.inverseBindMatrix;
    
    m_impl->skeleton->ModifyBone(index, b);
}

void Skeleton::RemoveBone(size_t index) {
    if (index >= m_impl->skeleton->BoneCount()) return;
    m_impl->skeleton->RemoveBone(index);
}

Skeleton::Bone Skeleton::GetBone(size_t index) const {
    Bone bone;
    if (index >= m_impl->skeleton->BoneCount()) return bone;
    
    const auto& b = m_impl->skeleton->GetBone(index);
    bone.name = b.name;
    bone.parentIndex = b.parentIndex;
    bone.bindPosition = b.bindPosition;
    bone.bindRotation = b.bindRotation;
    bone.bindScale = b.bindScale;
    bone.inverseBindMatrix = b.inverseBindMatrix;
    return bone;
}

void Skeleton::BuildBoneMap() {
    m_impl->skeleton->BuildBoneMap();
}

size_t Skeleton::BoneCount() const {
    return m_impl->skeleton->BoneCount();
}

int Skeleton::FindBoneIndex(const std::string& name) const {
    return m_impl->skeleton->FindBoneIndex(name);
}

std::shared_ptr<Skeleton> Skeleton::WrapInternal(const std::shared_ptr<eyelib::animation::Skeleton>& skeleton) {
    auto apiSkeleton = std::make_shared<Skeleton>();
    if (skeleton) apiSkeleton->m_impl->skeleton = skeleton;
    return apiSkeleton;
}

std::shared_ptr<eyelib::animation::Skeleton> Skeleton::GetInternal() const {
    return m_impl->skeleton;
}

} // namespace api
} // namespace eyelib
