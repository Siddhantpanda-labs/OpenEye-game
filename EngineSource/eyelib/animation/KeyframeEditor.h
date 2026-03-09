#pragma once

#include "AnimationClip.h"

namespace eyelib {
namespace animation {

class KeyframeEditor {
public:
    enum class Track { Position, Rotation, Scale };

    static void AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track);
    static void AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Quat& value);

    static void RemoveKeyframe(AnimationClip& clip, int boneIndex, float time, Track track);

    static void SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track);
    static void SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Quat& value);

    static size_t GetKeyframeCount(const AnimationClip& clip, int boneIndex);
};

} // namespace animation
} // namespace eyelib
