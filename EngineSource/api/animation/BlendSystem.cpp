#include "api/animation/BlendSystem.h"
#include "eyelib/animation/BlendSystem.h"

namespace eyelib {
namespace api {

std::shared_ptr<Pose> BlendSystem::SampleClip(const AnimationClip& clip, float time, size_t boneCount) {
    auto pose = std::make_shared<eyelib::animation::Pose>(boneCount);
    *pose = eyelib::animation::BlendSystem::SampleClip(*clip.GetInternal(), time, boneCount);
    return Pose::WrapInternal(pose);
}

std::shared_ptr<Pose> BlendSystem::Blend(const Pose& a, const Pose& b, float weight) {
    auto pose = std::make_shared<eyelib::animation::Pose>();
    *pose = eyelib::animation::BlendSystem::Blend(*a.GetInternal(), *b.GetInternal(), weight);
    return Pose::WrapInternal(pose);
}

std::shared_ptr<Pose> BlendSystem::BlendAdditive(const Pose& base, const Pose& additive, float weight) {
    auto pose = std::make_shared<eyelib::animation::Pose>();
    *pose = eyelib::animation::BlendSystem::BlendAdditive(*base.GetInternal(), *additive.GetInternal(), weight);
    return Pose::WrapInternal(pose);
}

std::shared_ptr<Pose> BlendSystem::BlendClips(const AnimationClip& a, const AnimationClip& b,
                                              float timeA, float timeB, size_t boneCount, float weight) {
    auto pose = std::make_shared<eyelib::animation::Pose>(boneCount);
    *pose = eyelib::animation::BlendSystem::BlendClips(*a.GetInternal(), *b.GetInternal(),
                                                       timeA, timeB, boneCount, weight);
    return Pose::WrapInternal(pose);
}

std::shared_ptr<Pose> BlendSystem::Blend1D(const std::vector<std::shared_ptr<Pose>>& poses,
                                           const std::vector<float>& weights) {
    std::vector<eyelib::animation::Pose> internal;
    internal.reserve(poses.size());
    for (const auto& p : poses) {
        internal.push_back(*p->GetInternal());
    }
    auto pose = std::make_shared<eyelib::animation::Pose>();
    *pose = eyelib::animation::BlendSystem::Blend1D(internal, weights);
    return Pose::WrapInternal(pose);
}

std::shared_ptr<Pose> BlendSystem::Blend2D(const std::vector<std::shared_ptr<Pose>>& poses,
                                           const std::vector<float>& weights) {
    std::vector<eyelib::animation::Pose> internal;
    internal.reserve(poses.size());
    for (const auto& p : poses) {
        internal.push_back(*p->GetInternal());
    }
    auto pose = std::make_shared<eyelib::animation::Pose>();
    *pose = eyelib::animation::BlendSystem::Blend2D(internal, weights);
    return Pose::WrapInternal(pose);
}

} // namespace api
} // namespace eyelib
