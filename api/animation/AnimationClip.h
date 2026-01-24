#pragma once

#include <memory>
#include <string>
#include <vector>
#include "eyelib/math/Types.h"
#include "eyelib/animation/AnimationClip.h"

namespace eyelib {
namespace api {

class AnimationClip {
public:
    AnimationClip();
    ~AnimationClip();

    AnimationClip(const AnimationClip&) = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;

    static std::shared_ptr<AnimationClip> Create(const std::string& name, float duration);

    void SetName(const std::string& name);
    void SetDuration(float duration);
    void SetLoop(bool loop);

    void AddPositionKey(int boneIndex, float time, const eyelib::math::Vec3& value);
    void AddRotationKey(int boneIndex, float time, const eyelib::math::Quat& value);
    void AddScaleKey(int boneIndex, float time, const eyelib::math::Vec3& value);

    void SetInterpolationMode(int boneIndex, int mode); // 0=Step,1=Linear,2=Cubic

    std::shared_ptr<eyelib::animation::AnimationClip> GetInternal() const;

private:
    class Impl;
    std::shared_ptr<Impl> m_impl;
    friend class Animator;
};

} // namespace api
} // namespace eyelib

