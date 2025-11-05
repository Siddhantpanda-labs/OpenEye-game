#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace OpenEye {
namespace Animation {

// Forward declarations
class AnimationClip;

/**
 * @brief Animation state machine - manages animation states and transitions.
 * 
 * Generic state machine for character/animation control:
 * - States: Named animation states (Idle, Walk, Run, Jump, etc.)
 * - Transitions: Rules for switching between states
 * - Conditions: Functions that evaluate when to transition
 * - Entry/Exit actions: Callbacks when entering/leaving states
 * 
 * This is a library-side component, reusable across the engine.
 */
class AnimationStateMachine {
public:
    /**
     * @brief Represents a state in the animation state machine.
     */
    struct State {
        std::string name;                    // State name (e.g., "Idle", "Walk")
        int clipIndex = -1;                  // Animation clip index (-1 = no clip)
        float playbackSpeed = 1.0f;          // Playback speed for this state
        bool loop = true;                     // Should animation loop
        
        // Entry/exit callbacks
        std::function<void()> onEnter;        // Called when entering this state
        std::function<void()> onExit;         // Called when leaving this state
        std::function<void(float)> onUpdate;  // Called every frame while in state (deltaTime)
        
        State() = default;
        State(const std::string& n, int clipIdx) : name(n), clipIndex(clipIdx) {}
    };
    
    /**
     * @brief Represents a transition between states.
     */
    struct Transition {
        std::string fromState;                // Source state name
        std::string toState;                  // Target state name
        float blendTime = 0.2f;               // Blend duration for transition
        
        // Condition function - returns true when transition should occur
        std::function<bool()> condition;
        
        // Optional transition callback
        std::function<void()> onTransition;
        
        Transition() = default;
        Transition(const std::string& from, const std::string& to, 
                  std::function<bool()> cond, float blend = 0.2f)
            : fromState(from), toState(to), blendTime(blend), condition(cond) {}
    };
    
    /**
     * @brief Default state name (played when no state is active)
     */
    static constexpr const char* DEFAULT_STATE = "Default";
    
    AnimationStateMachine();
    ~AnimationStateMachine() = default;
    
    // State management
    void AddState(const State& state);
    void AddState(const std::string& name, int clipIndex, float playbackSpeed = 1.0f, bool loop = true);
    bool HasState(const std::string& name) const;
    const State* GetState(const std::string& name) const;
    State* GetState(const std::string& name);
    
    // Transition management
    void AddTransition(const Transition& transition);
    void AddTransition(const std::string& fromState, const std::string& toState,
                      std::function<bool()> condition, float blendTime = 0.2f);
    
    // State machine control
    void SetInitialState(const std::string& stateName);
    void SetState(const std::string& stateName, float blendTime = 0.2f);
    const std::string& GetCurrentState() const { return m_currentState; }
    const std::string& GetPreviousState() const { return m_previousState; }
    
    // Update - call every frame to evaluate transitions
    void Update(float deltaTime);
    
    // Get current state's clip index and settings
    int GetCurrentClipIndex() const;
    float GetCurrentPlaybackSpeed() const;
    bool GetCurrentLoop() const;
    float GetCurrentBlendTime() const;
    
    // Check if state machine is valid
    bool IsValid() const;
    
    // Clear all states and transitions
    void Clear();
    
private:
    std::unordered_map<std::string, State> m_states;
    std::vector<Transition> m_transitions;
    
    std::string m_currentState;
    std::string m_previousState;
    std::string m_initialState;
    
    float m_pendingBlendTime = 0.2f;
    bool m_stateChanged = false;
    
    // Internal helpers
    void EnterState(const std::string& stateName);
    void ExitState(const std::string& stateName);
    void EvaluateTransitions();
};

} // namespace Animation
} // namespace OpenEye

