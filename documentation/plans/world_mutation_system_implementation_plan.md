# World Mutation System - Implementation Plan
Based on: `documentation/plans/world_mutation_system.md`

## Goals (non-negotiable)
- Only the World Mutation System writes to world state.
- All edits flow through commands (editor, AI, runtime, network).
- Commands are deterministic, serializable, reversible.
- One serializer for scene/save/snapshot.
- Editor and AI are just command producers.

---

## Module Placement
**Library**: `eyelib/` (core mutation system)  
**API**: `api/` (public command interface)  
**Apps**: `app/editor`, `app/*` (command producers)

---

## Directory Layout (target)
```
eyelib/
  world/
    World.h / World.cpp
    WorldState.h
    EntityRegistry.h / EntityRegistry.cpp
    ComponentStorage/
      IComponentStore.h
      ComponentStore.h
    WorldGraph/
      Hierarchy.h / Hierarchy.cpp
      Metadata.h / Metadata.cpp
      Tags.h / Tags.cpp
    Commands/
      Command.h
      CommandTypes.h
      CommandBuffer.h / CommandBuffer.cpp
      CommandValidator.h / CommandValidator.cpp
    Transactions/
      Transaction.h
      TransactionManager.h / TransactionManager.cpp
    Events/
      WorldEvents.h
      EventDispatcher.h / EventDispatcher.cpp
    Serialization/
      WorldSerializer.h / WorldSerializer.cpp
      WorldSnapshot.h / WorldSnapshot.cpp

api/
  world/
    World.h / World.cpp
    Commands.h / Commands.cpp
    Transactions.h / Transactions.cpp
    Serialization.h / Serialization.cpp
```

---

## Implementation Phases

### Phase 0 - Scaffolding (structure only)
- [x] Create folder layout in `eyelib/world/*` and `api/world/*`
- [x] Add headers/classes with clear responsibilities
- [x] Wire CMake targets

### Phase 1 - World State Core
**Objective**: Build the data model that can be serialized and mutated.
- [x] `WorldState` (data-only owner for registry + graph + stores)
- [x] `EntityRegistry` (stable IDs, create/destroy/isAlive)
- [x] Component storage (`ComponentStore<T>`)
- [x] World graph (`Hierarchy`, `Metadata`, `Tags`)

### Phase 2 - Commands + Buffer
**Objective**: The only legal mutation path.
- [x] `Command` base (`Apply`/`Revert`)
- [~] `CommandTypes`
  - [x] CreateEntity
  - [x] DestroyEntity
  - [x] ReparentEntity
  - [x] SetName (metadata)
  - [x] AddComponent (templated)
  - [x] RemoveComponent (templated)
  - [x] SetComponentData (templated)
  - [x] Serialization payload per command (including templated component commands)
- [x] `CommandBuffer` (queue + batch)

### Phase 3 - World Mutation Engine
**Objective**: One writer for world state.
- [~] `World`
  - [x] Owns `WorldState` + `CommandBuffer`
  - [x] Applies commands in order
  - [x] Emits events per command
  - [x] Exposes read-only views (`WorldView`)
  - [x] Scheduler (order + pause/step)

### Phase 4 - Validation & Events
**Objective**: Guardrails and reactive systems.
- [~] `CommandValidator`
  - [x] Valid entity references (basic)
  - [x] No cycles in hierarchy (basic)
  - [x] Required components (basic enforcement)
- [x] Events system skeleton (`WorldEvents`, `EventDispatcher`)

### Phase 5 - Transactions
**Objective**: Undo/redo + AI planning.
- [~] `Transaction`
  - [x] Command list
  - [x] Apply/Revert helpers
  - [ ] Inverse list (future)
- [~] `TransactionManager`
  - [x] Begin/Commit/Rollback
  - [x] Undo/Redo stacks (basic)

### Phase 6 - Serialization
**Objective**: One serializer for all snapshots.
- [~] `WorldSerializer`
  - [x] Serialize/deserialize `WorldState`
  - [x] Versioned schema
  - [x] Component serialization (Name, Transform, Renderable, Camera, Light, RigidBody, Animator, AABB, MeshRenderer, Collider, Script)
- [x] Region component for arena scoping (serialized)
- [~] `WorldSnapshot` (struct exists, no capture/restore yet)
  - [x] In-memory snapshot using serializer (json)

### Phase 7 - API Layer
**Objective**: Stable external interface.
- [~] `api/world/World` (submit/apply; read-only views pending)
- [x] `api/world/Commands` (helpers + templated component commands)
- [~] `api/world/Transactions` (basic wrapper)
- [x] `api/world/Serialization` (wrapper)

---

## Command Set (v1)
Mandatory:
- CreateEntity
- DestroyEntity
- AddComponent
- RemoveComponent
- SetComponentData
- ReparentEntity
- CreateRegion
- SetRegionData
- AttachRegionRules

Nice-to-have later:
- EnableSystemInRegion
 
## Added
- EnableSystemInRegion command (implemented)
- Region query helpers (point-in-region, priority select)
- Region runtime system (RegionMembership + RegionSystem)
- RegionMembership helpers + World update hook for RegionSystem
- RegionSystem applies per-region enabled systems to entities
- Scheduler + Observation layer
- Scheduler fixed timestep + substeps
- Default fixed timestep (1/60) + API controls
- Scheduler priorities/groups + time scale + replay mode

---

## Time Scale & Replay (Scheduler)
Use these controls to slow down, speed up, or replay deterministically:

- `SetTimeScale(scale)`
  - `1.0` normal, `0.5` half-speed, `2.0` double-speed
- `SetTimeMode(TimeMode::Realtime)`
  - Uses real `deltaSeconds` each frame
- `SetTimeMode(TimeMode::Fixed)`
  - Uses fixed timestep + substeps
- `SetTimeMode(TimeMode::Replay)`
  - Uses `SetReplayDelta(seconds)` per frame

Replay is deterministic if your commands + systems are deterministic.

---

## Determinism Rules
- Commands are applied in strict order of submission.
- No random behavior inside commands unless seeded.
- No direct system calls inside commands.

---

## Editor & AI Integration (no special cases)
Editor:
```
UI → Commands → CommandBuffer → World
```
AI:
```
Observation → Commands → CommandBuffer → World
```

---

## Completion Criteria
- World can be fully reconstructed from a snapshot.
- All mutations occur via commands only.
- Editor can create/move/delete entities with same command pipeline.
- AI can do the same with no direct memory access.

---

## Risks & Guardrails
- Avoid direct writes to component stores.
- Prevent hierarchy cycles in validator.
- Keep serialization format stable once shipped.
- No multiple serializers.

---

## Next Actions
- Add required-component rules (optional, later).
- Add tests for command apply + validator.
