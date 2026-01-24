#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace animation {

class AnimationClip;

struct AnimationEvent {
    float time = 0.0f;
    std::string name;
    std::string data;
};

class AnimationEvents {
public:
    using Callback = std::function<void(const AnimationEvent&)>;

    void AddEvent(const AnimationClip& clip, float time, const std::string& name, const std::string& data = "");
    std::vector<AnimationEvent> GetEvents(const AnimationClip& clip, float timeStart, float timeEnd) const;

    void SetCallback(Callback cb) { m_callback = std::move(cb); }
    void Dispatch(const AnimationClip& clip, float timeStart, float timeEnd) const;

private:
    std::unordered_map<const AnimationClip*, std::vector<AnimationEvent>> m_events;
    Callback m_callback{};
};

} // namespace animation
} // namespace eyelib
