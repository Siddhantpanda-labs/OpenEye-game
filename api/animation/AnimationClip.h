#pragma once

#include <memory>
#include <string>

namespace eyelib {
namespace api {

class AnimationClip {
public:
    AnimationClip();
    ~AnimationClip();

    AnimationClip(const AnimationClip&) = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;

    static std::shared_ptr<AnimationClip> Create(const std::string& name, float duration);

private:
    class Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib

