# Engine Architectural Standard

The transform layer has set the bar: **authority separation, derived state, versioning, determinism, persistent order, benchmark proof.**

Every other major subsystem must answer one brutal question:

> **Can you integrate with transforms at the same level of determinism, scalability, and architectural cleanliness?**

Below are the **non-negotiable questions** each system must pass. A system that cannot answer these cleanly does not match the transform architecture.

---

## Meta-standard (every system)

1. **“Do you mutate authoritative state directly, or do you go through the command + version system?”**
2. **“Can I run deterministically with transform propagation and produce identical results every run?”**

Required of all subsystems:

* Clear ownership (who is authoritative)
* Deterministic update order
* Versioned change tracking (where applicable)
* No hidden computation (no recomputing what is derived and cached)
* Benchmarkable performance

---

## 1. Physics System

Physics depends directly on transforms.

### Structural questions

1. **Who owns world transform during simulation?**  
   Does physics write back to TransformStore, or is transform authoritative and physics reads only?
2. **Is transform update order deterministic relative to physics step?**  
   e.g. `Commands → TransformSystem → PhysicsStep → WriteBackToTransform? → TransformSystem?`
3. **Are physics bodies stored separately from transform?**  
   Clear separation or tightly coupled?
4. **How do you prevent feedback loops?**  
   Transform changes → physics update → transform changes again?
5. **Is interpolation handled separately from authoritative world matrix?**

If physics cannot answer these cleanly, it does not match the transform architecture.

---

## 2. Culling / Rendering

Rendering reads world matrices. It must stay a pure consumer.

1. **Are world AABBs derived and cached like transforms?**
2. **Does culling operate on stable snapshot data only?**
3. **Is frustum culling deterministic across frames?**
4. **Does rendering ever compute transforms itself?**  
   If yes → architecture violation.
5. **Can the renderer run on a separate thread safely?**

Rendering must be: **pure consumer of derived state.**  
If it recomputes anything, that is architectural drift.

---

## 3. Networking

1. **What is authoritative? Local or world transform?**
2. **Are transform versions used for replication diffing?**
3. **Can you serialize only dirty transforms?**
4. **Does hierarchy change replicate deterministically?**
5. **Are reparent operations deterministic across clients?**

Networking must align with: version system, command system, deterministic transform propagation.  
Otherwise replay and multiplayer break.

---

## 4. AI / ML Training Loop

1. **Does observation produce a stable, snapshot-isolated world state?**
2. **Can transform updates be deterministic under fixed seed?**
3. **Can you step simulation without rendering?**
4. **Can you run 1000 worlds in parallel without shared state?**
5. **Is world state serializable and resettable quickly?**

ML requires: determinism, isolation, fast reset, no hidden side effects.  
The rest of the engine must match the transform system’s determinism.

---

## 5. Event System

1. Are transform changes logged as events?
2. Can systems subscribe to “transform changed” without recomputing?
3. Are events deterministic and replayable?
4. Do events avoid re-entrancy issues?

Event chaos destroys clean architecture fast.

---

## 6. Command System

The transform system depends on commands marking dirty correctly.

1. **Can any system mutate transforms outside commands?**  
   If yes, versioning collapses.
2. **Are batch commands atomic?**
3. **Is undo/redo consistent with versioning?**
4. **Are transactions deterministic?**

Any mutation that bypasses the command flow undermines the version system.

---

## 7. Scheduler

1. **Is update order fixed?**
2. **Can systems declare dependencies?**
3. **Can TransformSystem be guaranteed to run before dependent systems?**  
   TransformSystem must be a **registered Simulation-phase system**, not hidden inside command application. Physics declares runsAfter = ["TransformSystem"] (or TransformSystem runsBefore = ["PhysicsSystem"]).
4. **Can fixed timestep be enforced cleanly?**  
   Only Simulation phase runs in the fixed-step loop.
5. **Authority rule:** Only Simulation-phase systems may mutate authoritative simulation state. Transform local writes and world-matrix derivation happen in Simulation phase. Commands submitted during Simulation apply next frame. No derived updates in ProcessCommands.
6. **Graph lock:** After first Run(), the execution graph is frozen (no Register/Unregister). Determinism over flexibility.

Without this, physics + transform determinism collapses. See **SystemGraph_Scheduler_Design.md**.

---

## 8. Serialization

1. **Are world matrices serialized?**  
   They should not be — they are derived.
2. **Are version counters serialized?**  
   (Design choice: usually not; recompute on load.)
3. **Can hierarchy restore and trigger persistent linearized order rebuild?**
4. **Can replay restore exact world state?**

Derived state must not be persisted as authoritative.

---

## 9. Memory & Scale

1. **Are component arrays contiguous?**
2. **Can systems operate on SoA directly?**
3. **Is there zero per-frame allocation in steady state (for hot paths)?**
4. **Can 100k entities update under 2–3 ms (target)?**

If not, the system will not scale to the same level as the transform layer.

---

## The real question (execution order)

Before designing physics or render snapshot in detail, the next system must answer:

> **“Can execution order be declared, enforced, and audited?”**

If not, the rest of the architecture cannot match transform quality. The scheduler is the **architectural choke point**: it must be a **dependency-resolved execution graph with explicit phases**, not just a list of steps. See **`SystemGraph_Scheduler_Design.md`** for the contract (phases, RunsAfter/RunsBefore, DAG, fixed timestep, snapshot isolation).

---

## Next design targets

* **Scheduler (first):** SystemGraph-based scheduler with phases and dependency declaration. Design: **`documentation/plans/SystemGraph_Scheduler_Design.md`**. Until this is implemented and enforced, physics and rendering design are unstable.
* **Physics integration:** After scheduler is in place — authority, write-back, and step order with transforms.
* **Render snapshot pipeline:** After scheduler — culling on derived AABBs, stable snapshot, no transform computation in renderer.

This document is the bar. Every subsystem must meet it.

---

## Current alignment (brief)

* **Transform / Command / Observation:** Matches the standard (authoritative local, derived world, versioning, TransformSystem, observation read-only).
* **Commands:** All transform and hierarchy mutation goes through commands; SetTransform and Reparent mark dirty. Undo/redo exists; transactions are applied in order. **Open point:** ensure no code path mutates transform store or hierarchy outside commands.
* **Scheduler:** SystemGraph-based: ProcessCommands (apply pending only, no TransformSystem), PreSimulation (region), Simulation (TransformSystem then Physics when added) in fixed-step loop, PostSimulation, PreRender, RenderSnapshot. TransformSystem is a **registered Simulation-phase system**; no transform updates in command paths. Authority rule: only Simulation-phase systems mutate authoritative sim state. Graph locks after first Run(). See SystemGraph_Scheduler_Design.md.
* **Physics:** Physics-Authoritative model (see **Physics_Authority_Model.md**). PhysicsSystem runs in Simulation phase after TransformSystem; integrates linearVelocity → position; writes local transform; TransformSystemPostPhysics runs again. No transform writes outside Simulation. Minimal stub (motion only); determinism test (60 vs 144 FPS) passes. **Open:** collision, events, full integration with external Bullet bridge if used.
* **Rendering:** Uses snapshot / Observation; no transform computation in render path. **Gap:** world AABBs are not yet derived/cached; culling is not yet on a stable snapshot standard.
* **Serialization:** Local transforms and hierarchy are serialized; world matrices are not (derived). Version counters are not serialized; linearized order rebuilds on load. Aligned with "do not persist derived state."
* **Events:** Transform/hierarchy changes emit events (e.g. Reparented, ComponentChanged). Replay and determinism of event order need to be confirmed.
* **Memory/scale:** TransformStore is SoA; TransformSystem uses persistent linearized order and a single dirty pass with no per-entity allocation in steady state. Benchmark exists for 10k/100k.

Use this section as a baseline for the next design steps (scheduler, physics integration, render snapshot/culling).
