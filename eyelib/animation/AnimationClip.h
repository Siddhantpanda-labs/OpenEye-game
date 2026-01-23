#pragma once

#include "eyelib/math/Types.h"

#include <string>
#include <vector>

namespace eyelib {
namespace animation {

template <typename T>
struct Keyframe {
    float time = 0.0f;
    T value{};
};

struct AnimationChannel {
    int boneIndex = -1;
    std::vector<Keyframe<math::Vec3>> positionKeys;
    std::vector<Keyframe<math::Quat>> rotationKeys;
    std::vector<Keyframe<math::Vec3>> scaleKeys;
};

class AnimationClip {
public:
    AnimationClip() = default;
    AnimationClip(const std::string& name, float duration);

    void SetName(const std::string& name) { m_name = name; }
    const std::string& Name() const { return m_name; }

    void SetDuration(float duration) { m_duration = duration; }
    float Duration() const { return m_duration; }

    void SetLoop(bool loop) { m_loop = loop; }
    bool Loop() const { return m_loop; }

    void AddChannel(const AnimationChannel& channel);
    const AnimationChannel* FindChannel(int boneIndex) const;

    // Minimal sampling placeholder (full sampling/blending later)
    void SampleLocalPose(float /*time*/, std::vector<math::Mat4>& /*outLocal*/) const {}

private:
    std::string m_name{};
    float m_duration = 0.0f;
    bool m_loop = true;
    std::vector<AnimationChannel> m_channels;
};

} // namespace animation
} // namespace eyelib

