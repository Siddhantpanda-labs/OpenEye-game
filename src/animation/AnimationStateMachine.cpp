#include "AnimationStateMachine.h"
#include "core/log/Log.h"
#include <algorithm>

namespace OpenEye {
namespace Animation {

AnimationStateMachine::AnimationStateMachine()
    : m_currentState(DEFAULT_STATE)
    , m_previousState("")
    , m_initialState(DEFAULT_STATE)
    , m_pendingBlendTime(0.2f)
    , m_stateChanged(false)
{
}

void AnimationStateMachine::AddState(const State& state) {
    if (state.name.empty()) {
        OE_WARN("AnimationStateMachine: Cannot add state with empty name");
        return;
    }
    
    m_states[state.name] = state;
    
    // If this is the first state and no initial state set, use it as initial
    if (m_states.size() == 1 && m_initialState == DEFAULT_STATE) {
        m_initialState = state.name;
        m_currentState = state.name;
    }
}

void AnimationStateMachine::AddState(const std::string& name, int clipIndex, float playbackSpeed, bool loop) {
    State state;
    state.name = name;
    state.clipIndex = clipIndex;
    state.playbackSpeed = playbackSpeed;
    state.loop = loop;
    AddState(state);
}

bool AnimationStateMachine::HasState(const std::string& name) const {
    return m_states.find(name) != m_states.end();
}

const AnimationStateMachine::State* AnimationStateMachine::GetState(const std::string& name) const {
    auto it = m_states.find(name);
    if (it != m_states.end()) {
        return &it->second;
    }
    return nullptr;
}

AnimationStateMachine::State* AnimationStateMachine::GetState(const std::string& name) {
    auto it = m_states.find(name);
    if (it != m_states.end()) {
        return &it->second;
    }
    return nullptr;
}

void AnimationStateMachine::AddTransition(const Transition& transition) {
    if (transition.fromState.empty() || transition.toState.empty()) {
        OE_WARN("AnimationStateMachine: Cannot add transition with empty state names");
        return;
    }
    
    if (!HasState(transition.fromState)) {
        OE_WARN_F("AnimationStateMachine: Transition from unknown state: %s", transition.fromState.c_str());
        return;
    }
    
    if (!HasState(transition.toState)) {
        OE_WARN_F("AnimationStateMachine: Transition to unknown state: %s", transition.toState.c_str());
        return;
    }
    
    m_transitions.push_back(transition);
}

void AnimationStateMachine::AddTransition(const std::string& fromState, const std::string& toState,
                                         std::function<bool()> condition, float blendTime) {
    Transition transition;
    transition.fromState = fromState;
    transition.toState = toState;
    transition.condition = condition;
    transition.blendTime = blendTime;
    AddTransition(transition);
}

void AnimationStateMachine::SetInitialState(const std::string& stateName) {
    if (!HasState(stateName)) {
        OE_WARN_F("AnimationStateMachine: Cannot set initial state to unknown state: %s", stateName.c_str());
        return;
    }
    
    m_initialState = stateName;
    m_currentState = stateName;
    m_previousState = "";
}

void AnimationStateMachine::SetState(const std::string& stateName, float blendTime) {
    if (!HasState(stateName)) {
        OE_WARN_F("AnimationStateMachine: Cannot transition to unknown state: %s", stateName.c_str());
        return;
    }
    
    if (m_currentState == stateName) {
        return; // Already in this state
    }
    
    m_pendingBlendTime = blendTime;
    ExitState(m_currentState);
    m_previousState = m_currentState;
    m_currentState = stateName;
    m_stateChanged = true;
    EnterState(stateName);
}

void AnimationStateMachine::Update(float deltaTime) {
    if (m_currentState == DEFAULT_STATE || !HasState(m_currentState)) {
        return;
    }
    
    // Update current state
    auto* state = GetState(m_currentState);
    if (state && state->onUpdate) {
        state->onUpdate(deltaTime);
    }
    
    // Evaluate transitions from current state
    m_stateChanged = false;
    EvaluateTransitions();
    
    // Reset state changed flag after evaluation
    if (!m_stateChanged) {
        m_pendingBlendTime = 0.2f; // Default blend time
    }
}

int AnimationStateMachine::GetCurrentClipIndex() const {
    if (m_currentState == DEFAULT_STATE || !HasState(m_currentState)) {
        return -1;
    }
    
    const auto* state = GetState(m_currentState);
    return state ? state->clipIndex : -1;
}

float AnimationStateMachine::GetCurrentPlaybackSpeed() const {
    if (m_currentState == DEFAULT_STATE || !HasState(m_currentState)) {
        return 1.0f;
    }
    
    const auto* state = GetState(m_currentState);
    return state ? state->playbackSpeed : 1.0f;
}

bool AnimationStateMachine::GetCurrentLoop() const {
    if (m_currentState == DEFAULT_STATE || !HasState(m_currentState)) {
        return true;
    }
    
    const auto* state = GetState(m_currentState);
    return state ? state->loop : true;
}

float AnimationStateMachine::GetCurrentBlendTime() const {
    return m_pendingBlendTime;
}

bool AnimationStateMachine::IsValid() const {
    return !m_states.empty() && HasState(m_currentState);
}

void AnimationStateMachine::Clear() {
    m_states.clear();
    m_transitions.clear();
    m_currentState = DEFAULT_STATE;
    m_previousState = "";
    m_initialState = DEFAULT_STATE;
    m_pendingBlendTime = 0.2f;
    m_stateChanged = false;
}

void AnimationStateMachine::EnterState(const std::string& stateName) {
    auto* state = GetState(stateName);
    if (state && state->onEnter) {
        state->onEnter();
    }
}

void AnimationStateMachine::ExitState(const std::string& stateName) {
    auto* state = GetState(stateName);
    if (state && state->onExit) {
        state->onExit();
    }
}

void AnimationStateMachine::EvaluateTransitions() {
    // Check all transitions from current state
    for (const auto& transition : m_transitions) {
        if (transition.fromState == m_currentState) {
            // Check if condition is met
            if (transition.condition && transition.condition()) {
                // Transition!
                if (transition.onTransition) {
                    transition.onTransition();
                }
                
                m_pendingBlendTime = transition.blendTime;
                ExitState(m_currentState);
                m_previousState = m_currentState;
                m_currentState = transition.toState;
                m_stateChanged = true;
                EnterState(transition.toState);
                
                // Only take first valid transition
                break;
            }
        }
    }
}

} // namespace Animation
} // namespace OpenEye

