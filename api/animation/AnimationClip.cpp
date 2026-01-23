#include "api/animation/AnimationClip.h"

#include "eyelib/animation/AnimationClip.h"

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

} // namespace api
} // namespace eyelib

