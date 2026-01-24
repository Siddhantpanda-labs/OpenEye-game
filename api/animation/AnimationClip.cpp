#include "AnimationClip.h"

#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/KeyframeEditor.h"

namespace eyelib {
namespace api {

class AnimationClip::Impl {
public:
    explicit Impl(const std::string& name, float duration)
        : clip(std::make_shared<eyelib::animation::AnimationClip>(name, duration)) {}
    std::shared_ptr<eyelib::animation::AnimationClip> clip;
};

AnimationClip::AnimationClip() = default;
AnimationClip::~AnimationClip() = default;

std::shared_ptr<AnimationClip> AnimationClip::Create(const std::string& name, float duration) {
    auto apiClip = std::make_shared<AnimationClip>();
    apiClip->m_impl = std::make_shared<Impl>(name, duration);
    return apiClip;
}

void AnimationClip::SetName(const std::string& name) {
    if (m_impl && m_impl->clip) m_impl->clip->SetName(name);
}

void AnimationClip::SetDuration(float duration) {
    if (m_impl && m_impl->clip) m_impl->clip->SetDuration(duration);
}

void AnimationClip::SetLoop(bool loop) {
    if (m_impl && m_impl->clip) m_impl->clip->SetLoop(loop);
}

void AnimationClip::AddPositionKey(int boneIndex, float time, const eyelib::math::Vec3& value) {
    if (m_impl && m_impl->clip) {
        eyelib::animation::KeyframeEditor::AddKeyframe(*m_impl->clip, boneIndex, time, value,
            eyelib::animation::KeyframeEditor::Track::Position);
    }
}

void AnimationClip::AddRotationKey(int boneIndex, float time, const eyelib::math::Quat& value) {
    if (m_impl && m_impl->clip) {
        eyelib::animation::KeyframeEditor::AddKeyframe(*m_impl->clip, boneIndex, time, value);
    }
}

void AnimationClip::AddScaleKey(int boneIndex, float time, const eyelib::math::Vec3& value) {
    if (m_impl && m_impl->clip) {
        eyelib::animation::KeyframeEditor::AddKeyframe(*m_impl->clip, boneIndex, time, value,
            eyelib::animation::KeyframeEditor::Track::Scale);
    }
}

void AnimationClip::SetInterpolationMode(int boneIndex, int mode) {
    if (!m_impl || !m_impl->clip) return;
    using Mode = eyelib::animation::AnimationChannel::InterpolationMode;
    Mode m = Mode::Linear;
    if (mode == 0) m = Mode::Step;
    else if (mode == 2) m = Mode::Cubic;
    m_impl->clip->SetInterpolationMode(boneIndex, m);
}

std::shared_ptr<eyelib::animation::AnimationClip> AnimationClip::GetInternal() const {
    return m_impl ? m_impl->clip : nullptr;
}

} // namespace api
} // namespace eyelib

