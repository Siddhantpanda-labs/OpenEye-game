#include "eyelib/animation/AnimationClip.h"

namespace eyelib {
namespace animation {

AnimationClip::AnimationClip(const std::string& name, float duration)
    : m_name(name), m_duration(duration) {}

void AnimationClip::AddChannel(const AnimationChannel& channel) {
    m_channels.push_back(channel);
}

const AnimationChannel* AnimationClip::FindChannel(int boneIndex) const {
    for (const auto& ch : m_channels) {
        if (ch.boneIndex == boneIndex) return &ch;
    }
    return nullptr;
}

} // namespace animation
} // namespace eyelib

