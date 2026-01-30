#include "BlendSystem.h"

namespace eyelib {
namespace animation {

Pose BlendSystem::SampleClip(const AnimationClip& clip, float time, size_t boneCount) {
    Pose p(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        p.SetBoneTransform(i, clip.Sample(time, static_cast<int>(i)));
    }
    return p;
}

Pose BlendSystem::SampleClip(const AnimationClip& clip, float time, const Skeleton& skeleton) {
    const size_t boneCount = skeleton.BoneCount();
    Pose p(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        const Bone& b = skeleton.GetBone(i);
        if (clip.FindChannel(static_cast<int>(i))) {
            p.SetBoneTransform(i, clip.Sample(time, static_cast<int>(i)));
        } else {
            math::Transform bind;
            bind.position = b.bindPosition;
            bind.rotation = b.bindRotation;
            bind.scale = b.bindScale;
            p.SetBoneTransform(i, bind);
        }
    }
    return p;
}

Pose BlendSystem::Blend(const Pose& a, const Pose& b, float weight) {
    return a.Blend(b, weight);
}

Pose BlendSystem::BlendAdditive(const Pose& base, const Pose& additive, float weight) {
    return base.ApplyAdditive(additive, weight);
}

Pose BlendSystem::BlendClips(const AnimationClip& a, const AnimationClip& b,
                             float timeA, float timeB, size_t boneCount, float weight) {
    Pose pa = SampleClip(a, timeA, boneCount);
    Pose pb = SampleClip(b, timeB, boneCount);
    return pa.Blend(pb, weight);
}

Pose BlendSystem::Blend1D(const std::vector<Pose>& poses, const std::vector<float>& weights) {
    if (poses.empty() || poses.size() != weights.size()) return Pose{};
    Pose out = poses[0];
    float total = weights[0];
    for (size_t i = 1; i < poses.size(); ++i) {
        float w = weights[i];
        if (total + w <= 0.0f) continue;
        float t = w / (total + w);
        out.BlendInPlace(poses[i], t);
        total += w;
    }
    return out;
}

Pose BlendSystem::Blend2D(const std::vector<Pose>& poses, const std::vector<float>& weights) {
    return Blend1D(poses, weights);
}

} // namespace animation
} // namespace eyelib
