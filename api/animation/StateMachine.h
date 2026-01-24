#pragma once

#include "AnimationClip.h"
#include "eyelib/animation/AnimationStateMachine.h"
#include "eyelib/animation/StateMachineConditions.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

class StateMachine {
public:
    StateMachine();
    ~StateMachine();

    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    int AddState(const std::string& name, const std::shared_ptr<AnimationClip>& clip,
                 float playbackSpeed = 1.0f, bool loop = true);
    int AddTransition(int fromState, int toState,
                      const std::vector<std::shared_ptr<eyelib::animation::Condition>>& conditions,
                      float duration = 0.2f);

    void SetCondition(const std::string& name, bool value);
    void SetCondition(const std::string& name, float value);
    void SetTrigger(const std::string& name);

    void SetInitialState(int state);
    void Update(float deltaTime);

    std::shared_ptr<eyelib::animation::AnimationStateMachine> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Animator;
};

} // namespace api
} // namespace eyelib
