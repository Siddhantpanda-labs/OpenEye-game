#include "AnimationStateMachine.h"

namespace eyelib {
namespace animation {

AnimationStateMachine::StateHandle AnimationStateMachine::AddState(
    const std::string& name, std::shared_ptr<AnimationClip> clip,
    float playbackSpeed, bool loop) {
    State s;
    s.name = name;
    s.clip = std::move(clip);
    s.playbackSpeed = playbackSpeed;
    s.loop = loop;
    m_states.push_back(std::move(s));
    return static_cast<StateHandle>(m_states.size() - 1);
}

AnimationStateMachine::TransitionHandle AnimationStateMachine::AddTransition(
    StateHandle from, StateHandle to,
    const std::vector<std::shared_ptr<Condition>>& conditions,
    float duration, BlendMode mode) {
    Transition t;
    t.fromState = from;
    t.toState = to;
    t.duration = duration;
    t.blendMode = mode;
    t.conditions = conditions;
    m_transitions.push_back(std::move(t));
    return static_cast<TransitionHandle>(m_transitions.size() - 1);
}

void AnimationStateMachine::SetCondition(const std::string& name, bool value) {
    m_boolParams[name] = value;
}

void AnimationStateMachine::SetCondition(const std::string& name, float value) {
    m_floatParams[name] = value;
}

bool AnimationStateMachine::GetCondition(const std::string& name) const {
    auto it = m_boolParams.find(name);
    return it != m_boolParams.end() ? it->second : false;
}

float AnimationStateMachine::GetConditionFloat(const std::string& name) const {
    auto it = m_floatParams.find(name);
    return it != m_floatParams.end() ? it->second : 0.0f;
}

void AnimationStateMachine::SetTrigger(const std::string& name) {
    m_triggers[name] = true;
}

bool AnimationStateMachine::GetTrigger(const std::string& name) const {
    auto it = m_triggers.find(name);
    return it != m_triggers.end() && it->second;
}

void AnimationStateMachine::SetInitialState(StateHandle state) {
    m_currentState = state;
    m_previousState = -1;
    m_activeTransition = -1;
    m_stateTime = 0.0f;
    m_previousStateTime = 0.0f;
    m_transitionTime = 0.0f;
    m_currentPose = Pose(m_boneCount);
    ApplyStatePose(0.0f);
}

void AnimationStateMachine::Update(float deltaTime) {
    if (m_currentState < 0 || m_currentState >= static_cast<StateHandle>(m_states.size()))
        return;

    // Pass pre-advance time (time we displayed last frame) so first blend frame shows same pose, no replay/jerk
    float displayTime = m_stateTime;
    m_stateTime += deltaTime;
    EvaluateTransitions(displayTime);
    ApplyStatePose(deltaTime);
}

void AnimationStateMachine::EvaluateTransitions(float displayTime) {
    for (size_t i = 0; i < m_transitions.size(); ++i) {
        const auto& t = m_transitions[i];
        if (t.fromState != m_currentState) continue;
        bool ok = true;
        for (const auto& c : t.conditions) {
            if (c && !c->Evaluate(*this)) { ok = false; break; }
        }
        if (!ok) continue;

        // displayTime = time we displayed last frame (pre-advance m_stateTime) so we don't skip or replay a frame
        m_previousStateTime = displayTime;
        m_previousState = m_currentState;
        m_currentState = t.toState;
        m_activeTransition = static_cast<TransitionHandle>(i);
        m_activeTransitionDuration = t.duration;
        m_transitionTime = 0.0f;
        m_stateTime = 0.0f;  // Reset for new state
        ConsumeTriggersForTransition(t);
        break;
    }
}

void AnimationStateMachine::ApplyStatePose(float deltaTime) {
    if (m_currentState < 0 || m_currentState >= static_cast<StateHandle>(m_states.size())) return;
    const State& current = m_states[m_currentState];
    if (!current.clip) return;

    float t = m_stateTime * current.playbackSpeed;
    Pose currentPose = m_skeleton
        ? BlendSystem::SampleClip(*current.clip, t, *m_skeleton)
        : BlendSystem::SampleClip(*current.clip, t, m_boneCount);

    if (m_activeTransition >= 0 && m_previousState >= 0 &&
        m_previousState < static_cast<StateHandle>(m_states.size())) {
        const State& prev = m_states[m_previousState];
        if (prev.clip && m_activeTransitionDuration > 0.0f) {
            // Sample previous state at current time (before advancing) to avoid one-frame jump at transition start
            float prevTime = (m_previousStateTime + m_transitionTime) * prev.playbackSpeed;
            Pose prevPose = m_skeleton
                ? BlendSystem::SampleClip(*prev.clip, prevTime, *m_skeleton)
                : BlendSystem::SampleClip(*prev.clip, prevTime, m_boneCount);

            // Blend weight from current m_transitionTime (before advancing) so first frame uses w=0 = no jerk
            float w = m_transitionTime / m_activeTransitionDuration;
            if (w < 0.0f) w = 0.0f;
            if (w > 1.0f) w = 1.0f;

            m_currentPose = prevPose.Blend(currentPose, w);

            m_transitionTime += deltaTime;
            
            if (w >= 1.0f) {
                m_activeTransition = -1;
                m_previousState = -1;
                m_previousStateTime = 0.0f;
            }
            return;
        }
    }

    m_currentPose = std::move(currentPose);
}

float AnimationStateMachine::GetTransitionWeight() const {
    if (m_activeTransition < 0 || m_activeTransitionDuration <= 0.0f) return 0.0f;
    float w = m_transitionTime / m_activeTransitionDuration;
    if (w < 0.0f) return 0.0f;
    if (w > 1.0f) return 1.0f;
    return w;
}

void AnimationStateMachine::ConsumeTriggersForTransition(const Transition& t) {
    for (const auto& c : t.conditions) {
        auto* tc = dynamic_cast<TriggerCondition*>(c.get());
        if (tc) {
            m_triggers[tc->Name()] = false;
        }
    }
}

std::shared_ptr<AnimationClip> AnimationStateMachine::GetStateClip(StateHandle state) const {
    if (state < 0 || state >= static_cast<StateHandle>(m_states.size())) return nullptr;
    return m_states[state].clip;
}

float AnimationStateMachine::GetStatePlaybackSpeed(StateHandle state) const {
    if (state < 0 || state >= static_cast<StateHandle>(m_states.size())) return 1.0f;
    return m_states[state].playbackSpeed;
}

} // namespace animation
} // namespace eyelib
