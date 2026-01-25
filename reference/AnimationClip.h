#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace OpenEye {
namespace Animation {

/**
 * @brief Keyframe data for position, rotation, or scale.
 */
template<typename T>
struct Keyframe {
    float time = 0.0f;
    T value{};
    
    Keyframe() = default;
    Keyframe(float t, const T& v) : time(t), value(v) {}
};

/**
 * @brief Animation channel for a single bone.
 * 
 * Contains keyframes for position, rotation, and scale transforms.
 */
struct AnimationChannel {
    int boneIndex = -1;  // Which bone this channel animates
    
    // Keyframes
    std::vector<Keyframe<glm::vec3>> positionKeys;
    std::vector<Keyframe<glm::quat>> rotationKeys;
    std::vector<Keyframe<glm::vec3>> scaleKeys;
    
    // Sample animation at given time
    glm::vec3 SamplePosition(float time, bool loop = true) const;
    glm::quat SampleRotation(float time, bool loop = true) const;
    glm::vec3 SampleScale(float time, bool loop = true) const;
};

/**
 * @brief Complete animation clip with multiple bone channels.
 * 
 * Represents a single animation sequence (e.g., "Walk", "Run", "Jump").
 * Contains animation channels for each animated bone.
 */
class AnimationClip {
public:
    AnimationClip() = default;
    AnimationClip(const std::string& name, float duration);
    ~AnimationClip() = default;
    
    // Non-copyable (clips are typically unique)
    AnimationClip(const AnimationClip&) = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;
    
    // Movable
    AnimationClip(AnimationClip&&) noexcept = default;
    AnimationClip& operator=(AnimationClip&&) noexcept = default;
    
    // Properties
    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }
    
    void SetDuration(float duration) { m_duration = duration; }
    float GetDuration() const { return m_duration; }
    
    void SetLoop(bool loop) { m_loop = loop; }
    bool IsLooping() const { return m_loop; }
    
    // Channel management
    void AddChannel(const AnimationChannel& channel);
    size_t GetChannelCount() const { return m_channels.size(); }
    const AnimationChannel& GetChannel(size_t index) const { return m_channels[index]; }
    AnimationChannel& GetChannel(size_t index) { return m_channels[index]; }
    
    // Find channel by bone index
    const AnimationChannel* FindChannel(int boneIndex) const;
    AnimationChannel* FindChannel(int boneIndex);
    
    // Sample animation at given time (returns local transforms for each bone)
    void Sample(float time, std::vector<glm::mat4>& outLocalTransforms, 
                const std::vector<int>& boneIndices) const;
    
    // Clear all channels
    void Clear();

    // Helper: Find keyframe index for time (public for AnimationChannel)
    template<typename T>
    static int FindKeyframeIndex(const std::vector<Keyframe<T>>& keys, float time, bool loop);
    
    // Helper: Linear interpolation (public for AnimationChannel)
    template<typename T>
    static T Interpolate(const Keyframe<T>& k1, const Keyframe<T>& k2, float t);

private:
    std::string m_name;
    float m_duration = 0.0f;
    bool m_loop = true;
    std::vector<AnimationChannel> m_channels;
};

} // namespace Animation
} // namespace OpenEye

