#pragma once

#include "api/animation/AnimationClip.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

struct AnimationEvent {
    float time = 0.0f;
    std::string name;
    std::string data;
};

class AnimationEvents {
public:
    using Callback = std::function<void(const AnimationEvent&)>;

    AnimationEvents();
    ~AnimationEvents();

    AnimationEvents(const AnimationEvents&) = delete;
    AnimationEvents& operator=(const AnimationEvents&) = delete;

    void AddEvent(const AnimationClip& clip, float time, const std::string& name, const std::string& data = "");
    std::vector<AnimationEvent> GetEvents(const AnimationClip& clip, float timeStart, float timeEnd) const;

    void SetCallback(Callback cb);
    void Dispatch(const AnimationClip& clip, float timeStart, float timeEnd) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
