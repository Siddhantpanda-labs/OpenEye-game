#include "StateMachine.h"

namespace eyelib {
namespace api {

class StateMachine::Impl {
public:
    std::shared_ptr<eyelib::animation::AnimationStateMachine> sm = std::make_shared<eyelib::animation::AnimationStateMachine>();
};

StateMachine::StateMachine() : m_impl(std::make_unique<Impl>()) {}
StateMachine::~StateMachine() = default;

int StateMachine::AddState(const std::string& name, const std::shared_ptr<AnimationClip>& clip,
                           float playbackSpeed, bool loop) {
    auto internalClip = clip ? clip->GetInternal() : nullptr;
    return m_impl->sm->AddState(name, internalClip, playbackSpeed, loop);
}

int StateMachine::AddTransition(int fromState, int toState,
                                const std::vector<std::shared_ptr<eyelib::animation::Condition>>& conditions,
                                float duration) {
    return m_impl->sm->AddTransition(fromState, toState, conditions, duration);
}

void StateMachine::SetCondition(const std::string& name, bool value) {
    m_impl->sm->SetCondition(name, value);
}

void StateMachine::SetCondition(const std::string& name, float value) {
    m_impl->sm->SetCondition(name, value);
}

void StateMachine::SetTrigger(const std::string& name) {
    m_impl->sm->SetTrigger(name);
}

void StateMachine::SetInitialState(int state) {
    m_impl->sm->SetInitialState(state);
}

void StateMachine::Update(float deltaTime) {
    m_impl->sm->Update(deltaTime);
}

std::shared_ptr<eyelib::animation::AnimationStateMachine> StateMachine::GetInternal() const {
    return m_impl->sm;
}

} // namespace api
} // namespace eyelib
