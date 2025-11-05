# Animation State Machine

A generic, reusable animation state machine system for character animation control.

## Overview

The `AnimationStateMachine` provides a clean way to manage animation states and transitions for characters. It's designed as a library-side component, fully integrated with the ECS animation system.

## Features

- **States**: Named animation states (Idle, Walk, Run, Jump, etc.)
- **Transitions**: Conditional transitions between states with blend times
- **Entry/Exit Actions**: Callbacks when entering/leaving states
- **Update Callbacks**: Per-frame updates while in a state
- **Automatic Integration**: Works seamlessly with `AnimationSystem`

## Usage Example

```cpp
#include "animation/AnimationSystem.h"
#include "animation/AnimationStateMachine.h"

// Get entity with AnimationComponent
auto& animComponent = registry.get<ECS::AnimationComponent>(characterEntity);

// Create state machine
auto stateMachine = Animation::AnimationSystem::CreateStateMachine(registry, characterEntity);

// Add states (clipIndex corresponds to animation clip in AnimationComponent)
stateMachine->AddState("Idle", 0, 1.0f, true);      // clipIndex=0, speed=1.0, loop=true
stateMachine->AddState("Walk", 1, 1.0f, true);     // clipIndex=1, speed=1.0, loop=true
stateMachine->AddState("Run", 2, 1.2f, true);      // clipIndex=2, speed=1.2, loop=true
stateMachine->AddState("Jump", 3, 1.0f, false);    // clipIndex=3, speed=1.0, loop=false

// Add transitions with conditions
bool isMoving = false;
bool isRunning = false;
bool isJumping = false;

// Idle -> Walk (when moving)
stateMachine->AddTransition("Idle", "Walk", 
    [&]() { return isMoving && !isRunning; }, 
    0.2f);

// Walk -> Run (when running)
stateMachine->AddTransition("Walk", "Run",
    [&]() { return isRunning; },
    0.15f);

// Run -> Walk (when not running)
stateMachine->AddTransition("Run", "Walk",
    [&]() { return !isRunning && isMoving; },
    0.15f);

// Walk -> Idle (when stopped)
stateMachine->AddTransition("Walk", "Idle",
    [&]() { return !isMoving; },
    0.2f);

// Run -> Idle (when stopped)
stateMachine->AddTransition("Run", "Idle",
    [&]() { return !isMoving; },
    0.2f);

// Any -> Jump (when jumping)
stateMachine->AddTransition("Idle", "Jump",
    [&]() { return isJumping; },
    0.1f);
stateMachine->AddTransition("Walk", "Jump",
    [&]() { return isJumping; },
    0.1f);
stateMachine->AddTransition("Run", "Jump",
    [&]() { return isJumping; },
    0.1f);

// Jump -> Idle (when landing)
stateMachine->AddTransition("Jump", "Idle",
    [&]() { return !isJumping; },
    0.2f);

// Set initial state
stateMachine->SetInitialState("Idle");

// In your update loop:
// - Update isMoving, isRunning, isJumping based on input/physics
// - AnimationSystem::Update() will automatically update the state machine
//   and handle transitions
```

## State Callbacks

You can add entry, exit, and update callbacks to states:

```cpp
auto& idleState = stateMachine->GetState("Idle");
idleState->onEnter = []() {
    OE_INFO("Entered Idle state");
};

idleState->onExit = []() {
    OE_INFO("Exited Idle state");
};

idleState->onUpdate = [](float deltaTime) {
    // Called every frame while in Idle state
    // e.g., play idle variation animations
};
```

## Manual State Changes

You can also manually change states:

```cpp
// Force transition to a state
stateMachine->SetState("Jump", 0.1f);  // 0.1s blend time
```

## Integration with AnimationSystem

The `AnimationSystem` automatically:
- Updates the state machine each frame
- Applies state changes to the `AnimationComponent`
- Handles blend times for smooth transitions
- Updates playback speed and loop settings from states

No manual state machine updates needed - just call `AnimationSystem::Update()` as usual.

## Architecture

- **Library-side**: All animation code is in `src/animation/`
- **Generic**: State machine is reusable for any character/entity
- **ECS Integration**: Works with `AnimationComponent` and `AnimationSystem`
- **Type-safe**: Uses shared pointers for resource management

