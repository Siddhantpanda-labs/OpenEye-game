Good. Now we stop firefighting and fix this at architecture level.

You don’t want “another fix.”
You want a design that makes this class of bug impossible.

So I’ll give you two things:

1. **Architecture corrections (structural changes)**
2. **Concrete checklist to confirm no remaining error paths**

No fluff.

---

# PART 1 — Architecture Corrections

## 1️⃣ Separate Global Physics From World Instance

Right now, world and global Jolt state are still conceptually mixed.

Correct structure:

```
Engine
 └── PhysicsModule (process lifetime)
       ├── Global Jolt Init
       └── CreatePhysicsWorld()
             └── PhysicsWorldInstance
                   └── JoltBackend
                         └── JoltWorld (PhysicsSystem, JobSystem, bodies)
```

### Rules:

* Global allocator / Factory / type registration → live for engine lifetime.
* PhysicsWorldInstance → lives only during Play session.
* Play/Stop NEVER touches Jolt globals.
* Only Engine shutdown tears down Jolt globals.

If world teardown mutates global state → architecture is wrong.

---

## 2️⃣ Single-Entry World Shutdown (No Split Responsibility)

Right now shutdown is split across:

* Bridge
* Backend::Shutdown()
* Destructors

That is fragile.

You want:

```cpp
PhysicsWorldInstance::Destroy()
{
    StopSimulation();
    DetachListeners();
    DrainJobs();
    DestroyAllBodies();
    DestroyPhysicsSystem();
    DestroyJobSystem();
    ClearAllContainers();
}
```

Destructor must only assert state == Destroyed.

No hidden destruction logic in destructors.

Destructors should enforce invariants, not perform complex teardown.

---

## 3️⃣ Enforce Quiescent State Before Destruction

Before destroying PhysicsSystem:

You MUST guarantee:

* No worker thread running
* No callbacks firing
* No job queue entries pending
* No broadphase updates queued

So add explicit:

```cpp
jobSystem->WaitForAllJobs();
```

and ensure physics stepping is stopped.

If you can’t prove the world is quiescent, destruction is undefined.

---

## 4️⃣ Remove Cross-System Access During Shutdown

Shutdown order must be:

```
Disable scripting access to physics
Disable callbacks
Drain jobs
Destroy world
```

Never:

```
Destroy world while scripting still references it
```

Before physics shutdown:

```cpp
scripting->SetPhysicsBridge(nullptr);
```

Hard cut.

---

## 5️⃣ State Machine for World

Add:

```
Uninitialized
Running
ShuttingDown
Destroyed
```

Every public function asserts:

```
state == Running
```

No silent guards.

Fail fast.

---

## 6️⃣ No Static Globals in JoltWorld.cpp

No:

```
static bool s_joltGlobalInit;
```

Move that into PhysicsModule.

Global initialization must not be hidden inside world code.

---

## 7️⃣ Make Thread Ownership Explicit

JobSystem must either:

A) Be owned by Engine
or
B) Be fully owned and fully destroyed inside PhysicsWorldInstance

But in either case:

* No background threads outlive world.
* No background threads survive destruction boundary.

---

# PART 2 — Things To Confirm (Concrete Checklist)

This is the important part.

If all of these are true, your architecture is safe.

---

## ✅ 1. No Jobs Running During Destruction

Before destroying PhysicsSystem:

* Add temporary debug counter.
* Assert job queue empty.
* Assert no worker thread in flight.

If you can’t prove this → you still have race risk.

---

## ✅ 2. No Listener Attached During Destruction

Before world destruction:

```
physicsSystem->SetContactListener(nullptr);
```

If listener destructor touches ECS containers after they are destroyed → iterator corruption.

---

## ✅ 3. No Cross-Thread ECS Access

Confirm:

* Physics callbacks never access ECS containers directly.
* They enqueue events instead.
* ECS containers are only mutated on main thread.

If not, STL iterator debug will explode.

---

## ✅ 4. Runtime Flags Match

Confirm:

* Jolt static libs compiled with same:

  * `_ITERATOR_DEBUG_LEVEL`
  * `/MDd` or `/MTd`
  * Runtime library

Mismatch causes proxy corruption.

---

## ✅ 5. No Double Shutdown

Add debug logs to:

* WorldPhysicsBridge::Shutdown
* PhysicsWorld::Shutdown
* JoltBackend::Shutdown
* ~JoltWorld

Confirm each runs exactly once per Play session.

Double destroy of STL container = exactly this crash.

---

## ✅ 6. Stress Test Loop

Run:

```
for 50 cycles:
    Play
    Stop
```

If it survives 50 → lifecycle stable.

If it crashes randomly around iteration 7–12 → race condition.

---

## ✅ 7. Single-Thread Mode Test

Temporarily:

Disable JobSystemThreadPool (run single-threaded).

If crash disappears → 100% thread race.

That test is extremely powerful.

---

# The Core Principle

A subsystem must satisfy this invariant:

> It can be created and destroyed repeatedly without mutating process-global state and without relying on destruction order of unrelated systems.

If that is not true, architecture is fragile.

Right now your failures indicate teardown boundary is not quiescent.

---

# The Real Engineering Truth

Iterator debug is not your enemy.

It is proving your teardown boundary is not clean.

You must design the shutdown boundary like a transaction:

* Stop input
* Stop async work
* Drain
* Destroy
* Zero state

Anything less is undefined behavior.

---

# Final Summary

Architecture correction:

* Move Jolt globals to PhysicsModule (engine lifetime).
* Make world shutdown single-entry and explicit.
* Drain jobs before destruction.
* Detach listeners before destruction.
* Enforce state machine.
* No hidden destructor logic.

Things to confirm:

* No thread race
* No double destroy
* No cross-system access during teardown
* No runtime mismatch
* Survives 50 Play/Stop cycles
* Survives single-thread mode test

If you enforce these, this class of error disappears permanently.

If you want, next we can design the exact minimal PhysicsModule + PhysicsWorldInstance header structure cleanly.

---

# Implementation status (as of current codebase)

We follow the architecture above. This section records what is implemented and what remains.

## Implemented

### 1. Separate global physics from world instance
- **PhysicsModule** (`api/physics/PhysicsModule.h`, `.cpp`): engine subsystem that owns Jolt global init/shutdown. `Initialize()` calls `EnsureJoltGlobalsInitialized()`, `Shutdown()` calls `ShutdownJoltGlobals()`.
- **PhysicsGlobals** (`eyelib/physics/PhysicsGlobals.h`, `eyelib/physics/backend/jolt/JoltGlobals.cpp`): `EnsureJoltGlobalsInitialized()` and `ShutdownJoltGlobals()` handle allocator, Factory, `RegisterTypes` / `UnregisterTypes`. Single static flag; no globals in `JoltWorld.cpp`.
- **Engine registration**: Editor and Player call `Engine::RegisterSubsystem(std::make_unique<PhysicsModule>())` before `Engine::Initialize()`. Jolt globals are initialized once at engine start and shut down once at engine exit.
- **JoltWorld**: `~JoltWorld()` no longer tears down Jolt globals. It only runs `Shutdown()` (world instance) and `delete m_impl`. `EnsureJoltGlobalInit()` delegates to `EnsureJoltGlobalsInitialized()`.
- Play/Stop only create/destroy world instances; they do not reinitialize or tear down Jolt globals.

### 2. World shutdown behaviour (strict ownership)
- **PhysicsWorld** owns the backend. **One owner, one teardown, one state transition.** `Shutdown()` sets state to `ShuttingDown`, calls `backend->Shutdown()`, then `backend.reset()`, then state `Destroyed`. Destructor only asserts `m_state == Destroyed || m_state == Uninitialized` and frees `m_impl`; it does not call `Shutdown()` or run policy.
- **JoltBackend::Shutdown()**: Single place for backend teardown. Does `world.Shutdown()`, `shapes.Clear()`, then `delete m_impl; m_impl = nullptr`. Idempotent (`if (!m_impl) return`). **Destructor does NOT call Shutdown().** Destructor only frees memory if `m_impl` is still present (logs `[JoltBackend] destructor forced cleanup`). Backend is a resource; policy lives in PhysicsWorld.
- **JoltWorld::Shutdown()**: Single place for world-instance teardown: `SetContactListener(nullptr)`, enumerate and remove/destroy all bodies, then `m_initialized = false`. No hidden global teardown.
- **WorldPhysicsBridge::Shutdown()**: Destroys all bodies and shapes, clears `m_bindings`, calls `m_physics->Shutdown()`, then `m_physics.reset()`. Destructor calls `Shutdown()` (RAII).

### 3. Listener detached before destruction
- **JoltWorld::Shutdown()** sets `physicsSystem.SetContactListener(nullptr)` before removing bodies and before `Impl` (and thus `PhysicsSystem`) is destroyed.

### 4. No static globals in JoltWorld
- The former `s_joltGlobalInit` and global init logic live in `JoltGlobals.cpp` under the PhysicsGlobals API. `JoltWorld` only calls `EnsureJoltGlobalsInitialized()`.

### 5. Quiescent state before destruction (§3)
- **`DrainJobSystem()`** is called at the start of `JoltWorld::Shutdown()`. It uses Jolt’s `CreateBarrier()`, a single no-op job added to the barrier and released, `WaitForJobs(barrier)`, and `DestroyBarrier(barrier)` so that no physics jobs are in flight before the PhysicsSystem is destroyed.

### 6. Cross-system access during shutdown (§4)
- **`scripting->SetPhysicsBridge(nullptr)`** is called in `GameRuntime::Shutdown()` before `m_scripting.reset()` and before `m_physics->Shutdown()`, so scripting no longer references physics during teardown.
- **Editor Play/Stop**: Before destroying the game runtime, the editor calls `ctx.world.SetFrameSnapshot(nullptr)` and `ctx.world.SetCaptureSnapshotCallback(nullptr)` so the world does not hold a callback or pointer into the runtime. This prevents use-after-free on second Play (scheduler would otherwise invoke the old capture callback, which captured the destroyed `WorldRenderBridge`).

### 7. State machine for world (§5)
- **`JoltWorld::State`** enum: `Uninitialized`, `Running`, `ShuttingDown`, `Destroyed`. Set to `Running` after `Init()`, `ShuttingDown` at start of `Shutdown()`, `Destroyed` at end. Public methods that use the world (`Update`, `GetBodyInterface`, `GetGravity`, `SetGravity`, `GetPhysicsSystem`, etc.) assert `state == Running`.
- **`PhysicsWorld::State`** enum: `Uninitialized`, `Running`, `ShuttingDown`, `Destroyed`. Set to `Running` after `Initialize()`, `ShuttingDown` at start of `Shutdown()`, `Destroyed` at end. `Step()` and destructor assert as above; destructor does not run shutdown logic.

### 8. Debug logs for shutdown (Part 2 checklist)
- One log line per Play-session shutdown: `[WorldPhysicsBridge] Shutdown (per Play session)`, `[JoltBackend] Shutdown (per Play session)`, `[JoltWorld] Shutdown (per Play session)`. Use these to confirm each runs exactly once per Play/Stop and to detect double shutdown.

## Not yet implemented (optional or verification-only)

### 1. Single-entry world destroy (§2)
- Shutdown is a cascade: Bridge → PhysicsWorld → JoltBackend → JoltWorld. **Ownership is strict:** PhysicsWorld owns backend; only `PhysicsWorld::Shutdown()` runs backend shutdown. `JoltBackend::~JoltBackend()` does not call `Shutdown()` (only frees memory if still present). `PhysicsWorld::~PhysicsWorld()` only asserts state and frees `m_impl`. One teardown path, one log per subsystem per Play session.

### 2. Checklist items (Part 2) — verification
- **No jobs during destruction**: Done — `DrainJobSystem()` uses Jolt’s barrier and `WaitForJobs(barrier)` before tearing down the world.
- **Runtime flags match**: Jolt is built with `USE_STATIC_MSVC_RUNTIME_LIBRARY OFF` (see root `CMakeLists.txt`); build Jolt and the app in the same config (Debug/Release) so `_ITERATOR_DEBUG_LEVEL` and CRT match.
- **Double shutdown**: Use the shutdown log lines (`[WorldPhysicsBridge]`, `[JoltBackend]`, `[JoltWorld]`) to confirm each runs exactly once per Play session.
- **Stress test**: In the editor, run **50× Play → Stop** (e.g. press Play, wait 1–2 s, Stop; repeat 50 times). If it never crashes, lifecycle is stable. If it crashes around iteration 7–12, suspect a race.
- **Single-thread mode test**: Temporarily set `GetSafeWorkerThreadCount()` in `JoltWorld.cpp` to return `1` (or build Jolt with 1 worker) and run the same Play/Stop cycle. If the crash disappears, the failure is likely thread-related.

### How to run the stress test (50× Play/Stop)
1. Build and run the editor (e.g. `OpenEyeEditor`).
2. Load a scene that uses physics (entities with RigidBody + Collider).
3. Press **Play**, wait 1–2 seconds, press **Stop**. That is one cycle.
4. Repeat 50 times (or use a simple script/loop). If no crash, the physics lifecycle is stable.

### How to run the single-thread physics test
1. In `eyelib/physics/backend/jolt/JoltWorld.cpp`, change `GetSafeWorkerThreadCount()` to return `1` (so the job pool runs with one worker).
2. Rebuild, then run the same Play/Stop stress test. If a previous crash no longer occurs, the issue is likely related to multi-threaded job execution.

## File reference (current implementation)

| Concern | Location |
|--------|----------|
| Jolt global init/shutdown API | `eyelib/physics/PhysicsGlobals.h` |
| Jolt global init/shutdown impl | `eyelib/physics/backend/jolt/JoltGlobals.cpp` |
| Engine subsystem (PhysicsModule) | `api/physics/PhysicsModule.h`, `api/physics/PhysicsModule.cpp` |
| Physics world instance (Jolt) | `eyelib/physics/backend/jolt/JoltWorld.cpp`, `JoltBackend.cpp` |
| Bridge (ECS ↔ physics) | `app/player/WorldPhysics.cpp`, `WorldPhysics.h` |
| Game runtime shutdown order | `app/player/GameRuntime.cpp` (`Shutdown()`) |
| Editor/Player registration | `app/editor/EditorApp.cpp`, `app/player/PlayerApp.cpp` |
