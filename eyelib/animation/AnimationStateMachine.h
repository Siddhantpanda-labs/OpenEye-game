#pragma once

#include "AnimationClip.h"
#include "BlendSystem.h"
#include "Pose.h"
#include "Skeleton.h"
#include "StateMachineConditions.h"
#include "StateMachineTransitions.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace animation {

class AnimationStateMachine {
public:
    struct State {
        std::string name;
        std::shared_ptr<AnimationClip> clip;
        float playbackSpeed = 1.0f;
        bool loop = true;
    };

    using StateHandle = int;
    using TransitionHandle = int;

    StateHandle AddState(const std::string& name, std::shared_ptr<AnimationClip> clip,
                         float playbackSpeed = 1.0f, bool loop = true);
    TransitionHandle AddTransition(StateHandle from, StateHandle to,
                                   const std::vector<std::shared_ptr<Condition>>& conditions,
                                   float duration = 0.2f, BlendMode mode = BlendMode::Linear);

    void SetCondition(const std::string& name, bool value);
    void SetCondition(const std::string& name, float value);
    bool GetCondition(const std::string& name) const;
    float GetConditionFloat(const std::string& name) const;
    void SetTrigger(const std::string& name);
    /** Returns true if the named trigger was set this frame (used by TriggerCondition). */
    bool GetTrigger(const std::string& name) const;

    void SetInitialState(StateHandle state);
    void Update(float deltaTime);

    StateHandle GetCurrentState() const { return m_currentState; }
    Pose GetCurrentPose() const { return m_currentPose; }

    bool IsInTransition() const { return m_activeTransition >= 0; }
    /** When in transition, returns blend weight 0..1 (0=previous state, 1=current). Otherwise 0. */
    float GetTransitionWeight() const;

    void SetBoneCount(size_t count) { m_boneCount = count; }
    void SetSkeleton(const Skeleton* skeleton) { m_skeleton = skeleton; }
    float GetCurrentBlendTime() const { return m_activeTransitionDuration; }

private:
    void EvaluateTransitions();
    void ApplyStatePose(float deltaTime);
    void ConsumeTriggersForTransition(const Transition& t);

    std::vector<State> m_states;
    std::vector<Transition> m_transitions;

    std::unordered_map<std::string, bool> m_boolParams;
    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, bool> m_triggers;

    StateHandle m_currentState = -1;
    StateHandle m_previousState = -1;
    TransitionHandle m_activeTransition = -1;
    float m_transitionTime = 0.0f;
    float m_activeTransitionDuration = 0.0f;

    float m_stateTime = 0.0f;
    float m_previousStateTime = 0.0f;  // Track previous state's time for correct blending
    size_t m_boneCount = 0;
    const Skeleton* m_skeleton = nullptr;  // When set, SampleClip uses bind pose for bones without channels
    Pose m_currentPose;
};

} // namespace animation
} // namespace eyelib
