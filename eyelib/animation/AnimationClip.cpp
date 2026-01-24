#include "AnimationClip.h"
#include "eyelib/math/TransformUtils.h"

#include <algorithm>
#include <cmath>

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

AnimationChannel* AnimationClip::FindOrCreateChannel(int boneIndex) {
    for (auto& ch : m_channels) {
        if (ch.boneIndex == boneIndex) return &ch;
    }
    AnimationChannel ch;
    ch.boneIndex = boneIndex;
    m_channels.push_back(ch);
    return &m_channels.back();
}

float AnimationClip::NormalizeTime(float time) const {
    if (m_duration <= 0.0f) return 0.0f;
    if (!m_loop) {
        return std::min(std::max(time, 0.0f), m_duration);
    }
    float t = std::fmod(time, m_duration);
    if (t < 0.0f) t += m_duration;
    return t;
}

void AnimationClip::SetInterpolationMode(int boneIndex, AnimationChannel::InterpolationMode mode) {
    if (auto* ch = FindOrCreateChannel(boneIndex)) ch->interpolation = mode;
}

namespace {
template <typename T>
const Keyframe<T>* FindKeyframe(const std::vector<Keyframe<T>>& keys, float t, size_t& idx) {
    if (keys.empty()) return nullptr;
    if (t <= keys.front().time) { idx = 0; return &keys.front(); }
    if (t >= keys.back().time) { idx = keys.size() - 1; return &keys.back(); }
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].time && t <= keys[i + 1].time) { idx = i; return &keys[i]; }
    }
    idx = keys.size() - 1;
    return &keys.back();
}

math::Vec3 CatmullRom(const math::Vec3& p0, const math::Vec3& p1,
                      const math::Vec3& p2, const math::Vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }
} // namespace

math::Vec3 AnimationClip::SamplePosition(float time, int boneIndex) const {
    const AnimationChannel* ch = FindChannel(boneIndex);
    if (!ch || ch->positionKeys.empty()) return math::Vec3(0.0f);
    float t = NormalizeTime(time);
    size_t idx = 0;
    FindKeyframe(ch->positionKeys, t, idx);
    if (idx + 1 >= ch->positionKeys.size()) return ch->positionKeys[idx].value;
    const auto& k0 = ch->positionKeys[idx];
    const auto& k1 = ch->positionKeys[idx + 1];
    float dt = (k1.time - k0.time);
    float localT = (dt > 0.0f) ? (t - k0.time) / dt : 0.0f;
    switch (ch->interpolation) {
        case AnimationChannel::InterpolationMode::Step:
            return k0.value;
        case AnimationChannel::InterpolationMode::Cubic: {
            const auto& p0 = ch->positionKeys[(idx > 0) ? idx - 1 : idx].value;
            const auto& p1 = k0.value;
            const auto& p2 = k1.value;
            const auto& p3 = ch->positionKeys[(idx + 2 < ch->positionKeys.size()) ? idx + 2 : idx + 1].value;
            return CatmullRom(p0, p1, p2, p3, localT);
        }
        default:
            return glm::mix(k0.value, k1.value, localT);
    }
}

math::Quat AnimationClip::SampleRotation(float time, int boneIndex) const {
    const AnimationChannel* ch = FindChannel(boneIndex);
    if (!ch || ch->rotationKeys.empty()) return math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
    float t = NormalizeTime(time);
    size_t idx = 0;
    FindKeyframe(ch->rotationKeys, t, idx);
    if (idx + 1 >= ch->rotationKeys.size()) return ch->rotationKeys[idx].value;
    const auto& k0 = ch->rotationKeys[idx];
    const auto& k1 = ch->rotationKeys[idx + 1];
    float dt = (k1.time - k0.time);
    float localT = (dt > 0.0f) ? (t - k0.time) / dt : 0.0f;
    if (ch->interpolation == AnimationChannel::InterpolationMode::Step) {
        return k0.value;
    }
    if (ch->interpolation == AnimationChannel::InterpolationMode::Cubic) {
        localT = SmoothStep(localT);
    }
    return glm::slerp(k0.value, k1.value, localT);
}

math::Vec3 AnimationClip::SampleScale(float time, int boneIndex) const {
    const AnimationChannel* ch = FindChannel(boneIndex);
    if (!ch || ch->scaleKeys.empty()) return math::Vec3(1.0f);
    float t = NormalizeTime(time);
    size_t idx = 0;
    FindKeyframe(ch->scaleKeys, t, idx);
    if (idx + 1 >= ch->scaleKeys.size()) return ch->scaleKeys[idx].value;
    const auto& k0 = ch->scaleKeys[idx];
    const auto& k1 = ch->scaleKeys[idx + 1];
    float dt = (k1.time - k0.time);
    float localT = (dt > 0.0f) ? (t - k0.time) / dt : 0.0f;
    switch (ch->interpolation) {
        case AnimationChannel::InterpolationMode::Step:
            return k0.value;
        case AnimationChannel::InterpolationMode::Cubic: {
            const auto& p0 = ch->scaleKeys[(idx > 0) ? idx - 1 : idx].value;
            const auto& p1 = k0.value;
            const auto& p2 = k1.value;
            const auto& p3 = ch->scaleKeys[(idx + 2 < ch->scaleKeys.size()) ? idx + 2 : idx + 1].value;
            return CatmullRom(p0, p1, p2, p3, localT);
        }
        default:
            return glm::mix(k0.value, k1.value, localT);
    }
}

math::Transform AnimationClip::Sample(float time, int boneIndex) const {
    math::Transform t;
    t.position = SamplePosition(time, boneIndex);
    t.rotation = SampleRotation(time, boneIndex);
    t.scale = SampleScale(time, boneIndex);
    return t;
}

void AnimationClip::SampleLocalPose(float time, std::vector<math::Mat4>& outLocal) const {
    for (const auto& ch : m_channels) {
        if (ch.boneIndex < 0) continue;
        if (static_cast<size_t>(ch.boneIndex) >= outLocal.size()) {
            outLocal.resize(static_cast<size_t>(ch.boneIndex) + 1, math::Mat4(1.0f));
        }
        math::Transform t = Sample(time, ch.boneIndex);
        outLocal[static_cast<size_t>(ch.boneIndex)] = math::Compose(t.position, t.rotation, t.scale);
    }
}

} // namespace animation
} // namespace eyelib

