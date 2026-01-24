#include "Animator.h"

#include "eyelib/animation/Animator.h"

namespace eyelib {
namespace api {

class Animator::Impl {
public:
    std::shared_ptr<eyelib::animation::Animator> animator = std::make_shared<eyelib::animation::Animator>();
    std::vector<eyelib::math::Mat4> boneMatrices;
};

Animator::Animator() : m_impl(std::make_unique<Impl>()) {}
Animator::~Animator() = default;

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    if (!skeleton) return;
    m_impl->animator->SetSkeleton(skeleton->GetInternal());
}

void Animator::SetClip(std::shared_ptr<AnimationClip> clip) {
    if (!clip) return;
    m_impl->animator->SetClip(clip->GetInternal());
}

void Animator::SetStateMachine(std::shared_ptr<StateMachine> sm) {
    if (!sm) return;
    m_impl->animator->SetStateMachine(sm->GetInternal());
}

void Animator::SetBlendTree(std::shared_ptr<BlendTree> tree) {
    if (!tree) return;
    m_impl->animator->SetBlendTree(tree->GetInternal());
}

void Animator::Update(float deltaTime) {
    m_impl->animator->Update(deltaTime);
    m_impl->boneMatrices = m_impl->animator->GetBoneMatrices();
}

const std::vector<eyelib::math::Mat4>& Animator::GetBoneMatrices() const {
    return m_impl->boneMatrices;
}

void Animator::SetParameter(const std::string& name, float value) {
    m_impl->animator->SetParameter(name, value);
}

void Animator::SetParameter(const std::string& name, bool value) {
    m_impl->animator->SetParameter(name, value);
}

void Animator::SetTrigger(const std::string& name) {
    m_impl->animator->SetTrigger(name);
}

std::shared_ptr<Animator> Animator::WrapInternal(const std::shared_ptr<eyelib::animation::Animator>& animator) {
    auto apiAnimator = std::make_shared<Animator>();
    if (animator) apiAnimator->m_impl->animator = animator;
    return apiAnimator;
}

std::shared_ptr<eyelib::animation::Animator> Animator::GetInternal() const {
    return m_impl->animator;
}

} // namespace api
} // namespace eyelib
