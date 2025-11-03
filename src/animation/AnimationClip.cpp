#include "AnimationClip.h"
#include <algorithm>
#include <cmath>

namespace OpenEye {
namespace Animation {

AnimationClip::AnimationClip(const std::string& name, float duration)
    : m_name(name), m_duration(duration) {
}

void AnimationClip::AddChannel(const AnimationChannel& channel) {
    m_channels.push_back(channel);
}

const AnimationChannel* AnimationClip::FindChannel(int boneIndex) const {
    for (const auto& channel : m_channels) {
        if (channel.boneIndex == boneIndex) {
            return &channel;
        }
    }
    return nullptr;
}

AnimationChannel* AnimationClip::FindChannel(int boneIndex) {
    for (auto& channel : m_channels) {
        if (channel.boneIndex == boneIndex) {
            return &channel;
        }
    }
    return nullptr;
}

void AnimationClip::Sample(float time, std::vector<glm::mat4>& outLocalTransforms,
                          const std::vector<int>& boneIndices) const {
    if (m_channels.empty() || m_duration <= 0.0f) {
        return;
    }
    
    // Normalize time to [0, duration]
    if (m_loop && m_duration > 0.0f) {
        time = std::fmod(time, m_duration);
        if (time < 0.0f) {
            time += m_duration;
        }
    } else {
        time = std::clamp(time, 0.0f, m_duration);
    }
    
    // Sample each channel
    for (const auto& channel : m_channels) {
        if (channel.boneIndex < 0 || channel.boneIndex >= static_cast<int>(boneIndices.size())) {
            continue;
        }
        
        int boneIdx = channel.boneIndex;
        if (boneIdx >= static_cast<int>(outLocalTransforms.size())) {
            continue;
        }
        
        // Sample transforms
        glm::vec3 position = channel.SamplePosition(time, m_loop);
        glm::quat rotation = channel.SampleRotation(time, m_loop);
        glm::vec3 scale = channel.SampleScale(time, m_loop);
        
        // Build local transform matrix
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        
        outLocalTransforms[boneIdx] = transform;
    }
}

void AnimationClip::Clear() {
    m_channels.clear();
}

// ============================================================================
// AnimationChannel sampling implementations
// ============================================================================

glm::vec3 AnimationChannel::SamplePosition(float time, bool loop) const {
    if (positionKeys.empty()) {
        return glm::vec3(0.0f);
    }
    
    if (positionKeys.size() == 1) {
        return positionKeys[0].value;
    }
    
    // Find surrounding keyframes (using static method from AnimationClip)
    int index = AnimationClip::FindKeyframeIndex(positionKeys, time, loop);
    
    if (index < 0) {
        return positionKeys[0].value;
    }
    
    if (index >= static_cast<int>(positionKeys.size()) - 1) {
        return positionKeys.back().value;
    }
    
    // Interpolate
    const auto& k1 = positionKeys[index];
    const auto& k2 = positionKeys[index + 1];
    float t = (time - k1.time) / (k2.time - k1.time);
    return AnimationClip::Interpolate(k1, k2, t);
}

glm::quat AnimationChannel::SampleRotation(float time, bool loop) const {
    if (rotationKeys.empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    if (rotationKeys.size() == 1) {
        return rotationKeys[0].value;
    }
    
    int index = AnimationClip::FindKeyframeIndex(rotationKeys, time, loop);
    
    if (index < 0) {
        return rotationKeys[0].value;
    }
    
    if (index >= static_cast<int>(rotationKeys.size()) - 1) {
        return rotationKeys.back().value;
    }
    
    const auto& k1 = rotationKeys[index];
    const auto& k2 = rotationKeys[index + 1];
    float t = (time - k1.time) / (k2.time - k1.time);
    
    // Spherical linear interpolation for rotations
    return glm::slerp(k1.value, k2.value, t);
}

glm::vec3 AnimationChannel::SampleScale(float time, bool loop) const {
    if (scaleKeys.empty()) {
        return glm::vec3(1.0f);
    }
    
    if (scaleKeys.size() == 1) {
        return scaleKeys[0].value;
    }
    
    int index = AnimationClip::FindKeyframeIndex(scaleKeys, time, loop);
    
    if (index < 0) {
        return scaleKeys[0].value;
    }
    
    if (index >= static_cast<int>(scaleKeys.size()) - 1) {
        return scaleKeys.back().value;
    }
    
    const auto& k1 = scaleKeys[index];
    const auto& k2 = scaleKeys[index + 1];
    float t = (time - k1.time) / (k2.time - k1.time);
    return AnimationClip::Interpolate(k1, k2, t);
}

// ============================================================================
// Template helper implementations
// ============================================================================

template<typename T>
int AnimationClip::FindKeyframeIndex(const std::vector<Keyframe<T>>& keys, float time, bool loop) {
    if (keys.empty()) {
        return -1;
    }
    
    if (time <= keys[0].time) {
        return 0;
    }
    
    if (time >= keys.back().time) {
        return static_cast<int>(keys.size() - 1);
    }
    
    // Binary search for surrounding keyframes
    int left = 0;
    int right = static_cast<int>(keys.size() - 1);
    
    while (right - left > 1) {
        int mid = (left + right) / 2;
        if (keys[mid].time <= time) {
            left = mid;
        } else {
            right = mid;
        }
    }
    
    return left;
}

template<typename T>
T AnimationClip::Interpolate(const Keyframe<T>& k1, const Keyframe<T>& k2, float t) {
    // Linear interpolation (for vec3 positions/scales)
    // Note: Rotations use slerp in SampleRotation
    return k1.value + (k2.value - k1.value) * t;
}

// Explicit template instantiations
template int AnimationClip::FindKeyframeIndex<glm::vec3>(
    const std::vector<Keyframe<glm::vec3>>&, float, bool);
template int AnimationClip::FindKeyframeIndex<glm::quat>(
    const std::vector<Keyframe<glm::quat>>&, float, bool);

template glm::vec3 AnimationClip::Interpolate<glm::vec3>(
    const Keyframe<glm::vec3>&, const Keyframe<glm::vec3>&, float);
template glm::vec3 AnimationClip::Interpolate<glm::vec3>(
    const Keyframe<glm::vec3>&, const Keyframe<glm::vec3>&, float);

} // namespace Animation
} // namespace OpenEye

