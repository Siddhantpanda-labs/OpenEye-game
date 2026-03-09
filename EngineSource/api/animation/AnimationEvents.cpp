#include "api/animation/AnimationEvents.h"
#include "eyelib/animation/AnimationEvents.h"
#include <utility>

namespace eyelib {
namespace api {

class AnimationEvents::Impl {
public:
    eyelib::animation::AnimationEvents events;
    Callback callback;
};

AnimationEvents::AnimationEvents() : m_impl(std::make_unique<Impl>()) {}

AnimationEvents::~AnimationEvents() = default;

void AnimationEvents::AddEvent(const AnimationClip& clip, float time, const std::string& name, const std::string& data) {
    m_impl->events.AddEvent(*clip.GetInternal(), time, name, data);
}

std::vector<AnimationEvent> AnimationEvents::GetEvents(const AnimationClip& clip, float timeStart, float timeEnd) const {
    auto internal = m_impl->events.GetEvents(*clip.GetInternal(), timeStart, timeEnd);
    std::vector<AnimationEvent> out;
    out.reserve(internal.size());
    for (const auto& e : internal) {
        AnimationEvent evt;
        evt.time = e.time;
        evt.name = e.name;
        evt.data = e.data;
        out.push_back(std::move(evt));
    }
    return out;
}

void AnimationEvents::SetCallback(Callback cb) {
    m_impl->callback = std::move(cb);
    m_impl->events.SetCallback([this](const eyelib::animation::AnimationEvent& e) {
        if (!m_impl->callback) return;
        AnimationEvent evt;
        evt.time = e.time;
        evt.name = e.name;
        evt.data = e.data;
        m_impl->callback(evt);
    });
}

void AnimationEvents::Dispatch(const AnimationClip& clip, float timeStart, float timeEnd) const {
    m_impl->events.Dispatch(*clip.GetInternal(), timeStart, timeEnd);
}

} // namespace api
} // namespace eyelib
