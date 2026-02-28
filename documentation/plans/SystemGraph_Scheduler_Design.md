# SystemGraph-Based Scheduler Design

The transform layer is deterministic and versioned. The scheduler is the next architectural choke point: **execution order must be declared, enforced, and audited.** Until that exists, physics and rendering design are unstable.

This document specifies a **dependency-resolved execution graph with explicit phases**—the contract the rest of the engine will depend on.

---

## 1. Why Scheduler Must Come First

Today (after refactor):

* Commands mutate state (ProcessCommands applies pending only). TransformSystem runs **inside the Simulation phase** of the scheduler DAG—not in command application.
* Order is declared: TransformSystem is a registered Simulation-phase system; Physics (when added) runs after it via runsBefore.

What the scheduler guarantees:

* TransformSystem runs before physics (declared dependency).
* Snapshot capture happens after all simulation.
* No system runs twice per logical step (except Simulation × N in fixed-step loop).
* Fixed timestep: only Simulation phase runs in the accumulator loop.

A professional scheduler is not “call systems in a vector.” It is:

> **A dependency-resolved execution graph with explicit phases.**

Until that exists, physics and rendering design are guesswork.

---

## 2. Contract: What the Scheduler Must Guarantee

### 2.1 Explicit phases

Execution is divided into **phases**. No system chooses when it runs; the scheduler decides.

| Phase | Purpose | Runs | Fixed-step? |
|-------|---------|------|-------------|
| **ProcessCommands** | Apply pending commands **only**. No derived system updates (e.g. no TransformSystem here). | Once per frame | No |
| **PreSimulation** | After state is stable; before any simulation. | Once per frame | No |
| **Simulation** | TransformSystem, physics, gameplay. Runs multiple times per frame (fixed step). | N times (accumulator loop) | Yes |
| **PostSimulation** | After simulation; before render. Events, sync. | Once per frame | No |
| **PreRender** | Last chance to touch world before snapshot. | Once per frame | No |
| **RenderSnapshot** | Capture snapshot. Must see stable world; no pending commands. | Once per frame | No |

Order is fixed: **ProcessCommands → PreSimulation → (Simulation × N) → PostSimulation → PreRender → RenderSnapshot.**

### 2.2 Dependency declaration

Each system registers with:

* **Name** (unique)
* **Phase** (which phase it belongs to)
* **RunsAfter** (list of system names that must run before this one)
* **RunsBefore** (list of system names that must run after this one)
* **FixedStep** (if true and phase is Simulation, runs inside the fixed Δt loop; otherwise runs once per phase invocation)

The scheduler:

* Builds a **DAG** from phase order + RunsAfter/RunsBefore (within phase and across phases).
* **Validates** no cycles.
* **Topological sort** → deterministic execution list per phase.
* Same set of systems → **same order every run** (no reliance on registration order).

### 2.3 Deterministic order

* Execution order is a function of (phase, dependencies) only.
* No non-determinism from hash tables or registration order.
* Auditable: given a system set, the execution list is reproducible.

### 2.4 Fixed timestep enforcement

Physics (and any other fixed-step system) must run in a fixed Δt loop. The **scheduler** owns this, not physics:

```
accumulator += frameDelta * timeScale
while accumulator >= fixedStep && substeps < maxSubsteps:
    run Simulation phase (only systems in this phase that are FixedStep, or all in phase—see below)
    accumulator -= fixedStep
    substeps += 1
```

Design choice: either **all** Simulation-phase systems run each substep, or only those marked FixedStep. Recommendation: **all Simulation systems** run each substep (simpler; physics and other fixed-step logic live in that phase).

### 2.5 Snapshot isolation

**RenderSnapshot** runs only after:

* All simulation has run (Simulation × N and PostSimulation done).
* No pending commands that haven’t been applied (ProcessCommands runs once at frame start).
* TransformSystem has run (it is a Simulation-phase system, so it runs in the fixed-step loop before snapshot).

Snapshot must see a **stable world**. No system may run after snapshot for the current frame’s render.

### 2.6 Authority rule

**Only Simulation-phase systems are allowed to mutate authoritative simulation state.**

* Transform local writes and world-matrix derivation belong in Simulation phase (TransformSystem).
* Commands submitted during Simulation apply **next frame** (ProcessCommands runs once per frame, before PreSimulation).
* No hidden transform (or other derived) updates outside the scheduler DAG.

---

## 3. System Descriptor (Registration API)

```cpp
enum class SchedulePhase {
    ProcessCommands,  // scheduler-owned: apply pending commands only
    PreSimulation,
    Simulation,       // TransformSystem and Physics run here (fixed-step loop)
    PostSimulation,
    PreRender,
    RenderSnapshot
};

struct SystemDescriptor {
    std::string name;
    SchedulePhase phase = SchedulePhase::Simulation;
    std::vector<std::string> runsAfter;
    std::vector<std::string> runsBefore;
    bool fixedStep = true;   // only meaningful when phase == Simulation
    std::function<void(float)> execute;
};
```

Registration:

* Scheduler maintains a **graph**: nodes = systems, edges = “must run before”.
* Edges come from: (1) phase order (earlier phase → later phase), (2) RunsAfter (A runs after B → B → A), (3) RunsBefore (A runs before B → A → B).
* Cycle check: if cycle exists, registration fails (or assert).
* Per-phase execution list: topological order of nodes in that phase (and any cross-phase constraints), cached until graph changes.

### 3.1 Built-in phases (scheduler-owned, not registered)

* **ProcessCommands:** apply pending commands **only**. No TransformSystem or other derived updates; those run in Simulation phase. Always first.
* **RenderSnapshot:** capture world to snapshot (callback). Always after PostSimulation/PreRender.

**TransformSystem** is a **registered** Simulation-phase system (not inside ProcessCommands). It runs in the fixed-step loop with runsBefore = {"PhysicsSystem"} so physics always runs after transform update.

The declarative graph is PreSimulation, Simulation, PostSimulation, PreRender. ProcessCommands and RenderSnapshot are scheduler-owned callbacks.

---

## 4. Frame Loop (Canonical)

```
Frame(frameDelta):
    1. ProcessCommands()                     // apply pending commands only; no derived updates

    2. RunPhase(PreSimulation, frameDelta)   // once

    3. accumulator += frameDelta * timeScale
       while accumulator >= fixedStep && substeps < maxSubsteps:
           RunPhase(Simulation, fixedStep)   // TransformSystem, Physics, TransformSystemPostPhysics
           PostPhysicsStepCallback()        // per-substep; enqueue events only, no state mutation
           accumulator -= fixedStep
           substeps++

    4. RunPhase(PostSimulation, frameDelta)   // once

    5. RunPhase(PreRender, frameDelta)       // once

    6. CaptureRenderSnapshot()               // scheduler-owned; stable world

    7. (Rendering uses snapshot; not part of scheduler)
```

**Simulation loop rule:** Only Simulation-phase systems run inside the fixed-step loop. No transform or other derived updates outside this loop.

Determinism: same frameDelta, same commands, same systems → same execution and same world state at snapshot time.

---

## 5. Mapping Current World Scheduler to This Model

Current (`World.cpp`):

* `AddStep("mutation", "core", 0, ApplyPendingCommands)`
* `AddStep("region", "core", 10, RegionSystem::Update)`
* `Run(deltaSeconds)`: sort by (group, priority); if fixed timestep, accumulator loop runs **all** steps each substep.

Gaps:

* No explicit phases: “mutation” and “region” are just groups.
* No dependency declaration: TransformSystem runs inside mutation, but nothing declares “RegionSystem runs after TransformSystem.”
* Fixed timestep runs the whole list (including mutation) each substep—so commands would be applied multiple times per frame if we’re not careful. In practice mutation runs once per frame in the caller’s design; the current Run() either runs once (realtime) or runs all steps in a loop (fixed). So currently “mutation” runs every substep in fixed mode—likely wrong. So the current scheduler does **not** guarantee “commands once, then simulation N times.”
* Snapshot capture is not a scheduler phase; it’s done by the app after World::Update(). So “snapshot sees stable world” is only true if the app never calls anything else after Update() before capturing—not enforced.

Migration:

* Introduce Phase enum and SystemDescriptor.
* Register “mutation” as ProcessCommands (scheduler-owned, not a descriptor).
* Register RegionSystem as PreSimulation or Simulation with runsAfter ["TransformSystem"] (or equivalent).
* Move snapshot capture into the scheduler as a dedicated phase (or a single step after PostSimulation/PreRender).
* Fixed timestep loop: run **only** Simulation phase (and optionally PostSimulation once after the loop). ProcessCommands and PreSimulation run once per frame before the loop.

---

## 6. DAG Resolution Algorithm

1. **Nodes:** All registered systems (by name) plus synthetic nodes for phase boundaries if needed.
2. **Edges (A → B means A runs before B):**
   * For each system S in phase P, add edge (any system in earlier phase) → S.
   * For each S, for each name N in runsAfter: add edge N → S.
   * For each S, for each name M in runsBefore: add edge S → M.
3. **Cycle detection:** DFS or Kahn; if cycle, fail registration.
4. **Topological sort** per phase: so within Simulation (and others), order is deterministic. Cache the resulting execution list per phase; invalidate on Register/Unregister.

---

## 7. What This Enables

Once the scheduler enforces this:

* **Physics** can declare: phase = Simulation, runsAfter = ["TransformSystem"], fixedStep = true. So it always runs after transform update, in fixed steps.
* **Rendering** can assume: snapshot is taken after PostSimulation and PreRender; no need to compute transforms; world AABBs (when added) will be derived from the same stable state.
* **Events:** If event dispatch is a system (e.g. PostSimulation), order is fixed relative to simulation and snapshot.
* **Audit:** “Can execution order be declared, enforced, and audited?” → Yes: phases + DAG + deterministic sort.

---

## 8. Implementation Notes (Summary)

* **WorldScheduler** (or a new **SystemGraphScheduler**) holds:
  * Phase order (constant).
  * Registered SystemDescriptors (name → descriptor).
  * Graph: name → { names that must run before this }.
  * Cached execution lists per phase (vector of name + fn).
* **Register(SystemDescriptor):** add node, add edges from phase + runsAfter/runsBefore, cycle check, rebuild topological order. **Fails if graph is locked** (see below).
* **Run(frameDelta):** execute as in §4; RunPhase(phase, dt) runs the cached list for that phase in order. **After first Run(), the graph is locked**—no further Register/Unregister. Determinism over flexibility.
* **ProcessCommands** and **CaptureRenderSnapshot** are callbacks the scheduler invokes at the right moment (owned by World or app).
* **PostPhysicsStepCallback:** Invoked once **per substep** after RunPhase(Simulation) inside the fixed-step loop. Use for enqueueing events only (e.g. collision); must not mutate simulation state.
* **Graph lock:** After the first call to Run(), the execution graph is frozen. Register() and Unregister() no-op or return false. This prevents runtime graph changes that could compromise deterministic order.
* **Explicit flush for tooling:** ApplyCommandSync/ApplyTransaction no longer run TransformSystem. World matrices update only in Simulation phase. For editor/tools that need immediate world-space reads after applying commands, the API provides **World::FlushSimulation()** (or **RunSimulationPhaseOnce(dt)** on the scheduler), which runs only the Simulation phase once (e.g. with dt=0). Explicit only—no hidden updates in command paths.

### Physics Authority Contract

See **Physics_Authority_Model.md**. Summary:

* Physics owns motion integration; TransformSystem owns world matrix derivation.
* Write-back (physics writing local transform) occurs before the next TransformSystem run in the same substep.
* All state mutation inside Simulation phase only. No transform or physics writes outside it.

---

## 9. Acceptance Criteria

The scheduler design is “locked down” when:

1. **Explicit phases** exist and are documented (ProcessCommands, PreSimulation, Simulation, PostSimulation, PreRender, RenderSnapshot).
2. **Dependency declaration** is supported (RunsAfter, RunsBefore); cycle detection prevents invalid graphs.
3. **Deterministic order** is guaranteed (topological sort, same graph → same order).
4. **Fixed timestep** is enforced by the scheduler (accumulator loop over Simulation phase only).
5. **Snapshot isolation** is guaranteed (snapshot runs after all simulation, no pending commands, stable world).

After that, physics and render snapshot can be designed against this contract. No system should run without declaring phase and dependencies; the scheduler is the single place that enforces “execution order is declared, enforced, and audited.”
