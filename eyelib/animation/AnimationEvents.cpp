#include "AnimationEvents.h"

#include "AnimationClip.h"

#include <algorithm>

namespace eyelib {
namespace animation {

void AnimationEvents::AddEvent(const AnimationClip& clip, float time, const std::string& name, const std::string& data) {
    auto& list = m_events[&clip];
    list.push_back(AnimationEvent{ time, name, data });
    std::sort(list.begin(), list.end(), [](const AnimationEvent& a, const AnimationEvent& b) {
        return a.time < b.time;
    });
}

std::vector<AnimationEvent> AnimationEvents::GetEvents(const AnimationClip& clip, float timeStart, float timeEnd) const {
    std::vector<AnimationEvent> out;
    auto it = m_events.find(&clip);
    if (it == m_events.end()) return out;
    const auto& list = it->second;
    for (const auto& e : list) {
        if (e.time >= timeStart && e.time <= timeEnd) out.push_back(e);
    }
    return out;
}

void AnimationEvents::Dispatch(const AnimationClip& clip, float timeStart, float timeEnd) const {
    if (!m_callback) return;
    auto events = GetEvents(clip, timeStart, timeEnd);
    for (const auto& e : events) m_callback(e);
}

} // namespace animation
} // namespace eyelib
