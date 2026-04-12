# World Module — Structure & Architecture

This document describes the structure and architecture of the **world module** (**eyelib/world**): ECS state, transform system, hierarchy, commands, observation, and serialization.

---

## 1. Overview

The world module provides:

- **Entity–component state** — entities, component stores (SoA where used), and a dedicated **TransformStore** for local TRS and cached world TRS.
- **Transform system** — local TRS is authoritative; world TRS is cached; matrices are derived only when needed. Algebraic TRS composition (Unreal-style) and dirty propagation.
- **Hierarchy** — explicit parent/child graph with cycle prevention; used for transform propagation and scene structure.
- **Commands** — mutable operations (create entity, add/remove component, set transform, reparent) as reversible command objects applied to `WorldState`.
- **Observation** — read-only views over the world (transforms, renderables, lights, cameras, etc.) for rendering and gameplay.
- **Serialization** — save/load world state (components, hierarchy, metadata) to/from JSON.

Design principles:

- **Local TRS authoritative** — Position, rotation, scale are stored per entity; world transform is derived and cached.
- **Safety on write** — All transform writes (SetTransform, Add, Reparent with preserve-world) go through sanitization: NaN replacement, quaternion normalization, scale clamp (min epsilon).
- **No matrix as source of truth** — Matrices are built from TRS when needed (e.g. for rendering); reparent uses TRS inverse + compose, not matrix decompose.

---

## 2. Directory Layout

```
eyelib/world/
├── World.h / World.cpp              # Public API: ApplyCommandSync, SetParent, GetParent, LoadScene, SaveScene, CaptureRenderSnapshot
├── WorldState.h / WorldState.cpp     # Mutable state: EntityRegistry, Hierarchy, Metadata, Tags, ComponentStores, TransformStore
├── WorldView.h                      # Read-only view over WorldState (State, Entities, GraphHierarchy, Components, TryGetComponent)
├── EntityId.h                       # Entity identifier (index + generation)
├── EntityRegistry.h / .cpp          # Entity lifecycle (create, destroy, alive set); EnTT mapping
├── ComponentStorage/
│   ├── IComponentStore.h            # Interface (Add, Remove, Has, Clear)
│   ├── ComponentStore.h / .inl     # Generic AoSoA component store (type-keyed)
│   └── TransformStore.h            # SoA transform store: local TRS + world TRS cache + world matrices (derived)
├── WorldGraph/
│   ├── Hierarchy.h / .cpp           # Parent/child graph: SetParent, RemoveParent, GetParent, GetChildren, WouldCreateCycle
│   ├── Metadata.h / .cpp           # Per-entity name and flags
│   └── Tags.h / .cpp               # Per-entity tags
├── Commands/
│   ├── Command.h                   # Interface: Apply(state), Revert(state), GetName()
│   ├── CommandTypes.h              # CreateEntity, DestroyEntity, AddComponent, RemoveComponent, SetTransform, ReparentEntity, etc.
│   ├── CommandBuffer.h / .cpp      # Record and apply command lists
│   ├── CommandRecord.h / .cpp      # Serialize/deserialize commands (e.g. for undo)
│   └── CommandValidator.cpp        # Pre-apply validation (e.g. cycle check for Reparent)
├── Transactions/
│   ├── Transaction.h                # Named command batch
│   ├── TransactionManager.h / .cpp # Register and run transactions
│   └── ...
├── systems/
│   ├── IWorldSystem.h               # Interface for world systems
│   ├── TransformSystem.h / .cpp    # Single-pass world TRS update (linearized hierarchy order)
│   └── RegionSystem.h / .cpp       # Region and membership
├── observation/
│   ├── WorldObservation.h / .cpp   # Read-only observation: GetTransform (world), GetXxxView (renderables, lights, etc.)
│   └── ...
├── Serialization/
│   ├── WorldSerializer.h / .cpp    # Save/load world state to JSON
│   ├── WorldSerializationTraits.h # Per-component Serialize/Deserialize (Transform, Renderable, Light, ...)
│   ├── ComponentSerializer.h / .cpp
│   └── ...
├── Query.h / Query.cpp             # Entity queries
├── RegionQueries.h / .cpp          # Spatial/region queries
├── Events/
│   ├── WorldEvents.h
│   ├── EventDispatcher.h / .cpp
│   └── EventLog.h / .cpp
├── scheduler/                      # System graph and scheduling (optional)
└── WORLD_MODULE.md                 # This file
```

Components (e.g. **Transform**, **Renderable**, **Light**, **Camera**, **RigidBody**, **Collider**, **MeshRenderer**, **Animator**, **Script**, **Name**, etc.) live in **eyelib/world/components/** and are stored via **ComponentStore&lt;T&gt;** or, for transforms, the dedicated **TransformStore**.

---

## 3. Transform System — Design & Architecture

### 3.1 Data (TransformStore)

- **Local TRS (authoritative)**  
  SoA arrays: `m_positions`, `m_rotations`, `m_scales`. Same order as `m_entityIds`. Written only through the store’s `Add`, `TransformRef::operator=`, and commands; all writes are sanitized.

- **World TRS (cached)**  
  SoA arrays: `m_worldPositions`, `m_worldRotations`, `m_worldScales`. Filled by **TransformSystem** using algebraic TRS composition. Never used as source of truth for hierarchy math.

- **World matrices (derived)**  
  `m_worldMatrices[i]` = `Compose(worldPos[i], worldRot[i], worldScale[i])`. Set in **SetWorldTRSByIndex** when world TRS is written. Used by rendering and other consumers that need a matrix.

- **Dirty tracking**  
  `m_localDirtyVersion`, `m_worldComputedVersion` per index; `m_globalTransformVersion`. **MarkLocalDirty(entity)** and **MarkSubtreeDirty(hierarchy, root)** propagate dirty so world TRS is recomputed only when needed.

### 3.2 TRS Math (eyelib/math/TransformUtils.h)

- **ComposeTRS(parentPos, parentRot, parentScale, localPos, localRot, localScale → outWorldPos, outWorldRot, outWorldScale)**  
  Unreal-style composition without matrices:
  - `world.rotation = parent.rotation * local.rotation` (then normalized)
  - `world.scale = parent.scale * local.scale` (component-wise)
  - `world.position = parent.rotation * (local.position * parent.scale) + parent.position`

- **InverseTRS(position, rotation, scale → outInvPos, outInvRot, outInvScale)**  
  TRS inverse for reparent: `inv.scale = 1/scale`, `inv.rotation = inverse(rotation)`, `inv.position = -(inv.rotation * (position * inv.scale))`.

- **Compose(position, rotation, scale) → Mat4**  
  Builds matrix from TRS for rendering; inputs are sanitized inside Compose.

- **Safety helpers**  
  - **ClampScale(scale)** — each component ≥ `kMinScale` (0.0001).
  - **SanitizePosition**, **SanitizeRotation**, **SanitizeScale** — replace NaN with safe defaults (0, identity quat, 1 then ClampScale).
  - **ContainsNaN(Vec3/Quat)** — for validation if needed.

All transform **writes** (SetTransformCommand, TransformStore::Add, TransformRef::operator=, ReparentEntityCommand when setting new local, SetWorldTRSByIndex) use the sanitizers so stored data is never NaN and scale is never zero.

### 3.3 Update Algorithm (TransformSystem)

1. **Linearized order**  
   Rebuild when hierarchy or transform count changes: parent-before-child order so each entity’s parent world TRS is already up to date.

2. **Single pass**  
   For each entity in that order: if dirty or parent updated, read local TRS from store; if root, world TRS = local; else world TRS = **ComposeTRS(parentWorldTRS, localTRS)**. Write world TRS (and derived matrix) via **SetWorldTRSByIndex**; mark world version and “updated this pass.”

3. **Reparent (preserve world)**  
   In **ReparentEntityCommand**: read child and new parent world TRS from store; `newLocalTRS = ComposeTRS(InverseTRS(parentWorldTRS), childWorldTRS)`; set child’s local TRS to newLocalTRS (through sanitizers). No matrix or Decompose.

### 3.4 Hierarchy (WorldGraph/Hierarchy)

- **Storage** — `m_parent` (entity → parent), `m_children` (entity → vector of children). Version bumped on SetParent/RemoveParent/Clear for cache invalidation.
- **SetParent(child, parent)** — removes child from previous parent, sets new parent, rejects if child == parent. **CommandValidator** calls **WouldCreateCycle(child, parent)** before Apply; if true, reparent is rejected.
- **WouldCreateCycle** — walks parent chain from `parent`; returns true if `child` is reached (would create a cycle).

---

## 4. Commands & Transactions

### 4.1 Command Interface (Commands/Command.h)

- **Apply(WorldState&)** — perform the mutation; return true if successful.
- **Revert(WorldState&)** — undo (optional; e.g. SetTransform has no revert).
- **GetName()** — command type name for debugging/recording.

### 4.2 Main Command Types (CommandTypes.h)

- **CreateEntityCommand** — create entity with optional default Transform; add to registry.
- **DestroyEntityCommand** — remove entity and all components (including transform).
- **AddComponent&lt;T&gt;** / **RemoveComponent&lt;T&gt;** — add/remove component for an entity.
- **SetTransformCommand** — set local TRS for an entity; value is sanitized (position, rotation, scale) then applied; **MarkLocalDirty**.
- **ReparentEntityCommand** — set parent of child; if **preserveWorldTransform**, compute new local via **InverseTRS** + **ComposeTRS** and set child local (sanitized); then **SetParent** and **MarkSubtreeDirty**.
- **SetNameCommand**, **AddTagCommand**, etc. — metadata and tags.

Commands that change transform or hierarchy call **MarkLocalDirty** or **MarkSubtreeDirty** so **TransformSystem** recomputes world TRS on the next update.

### 4.3 CommandBuffer & Validation

- **CommandBuffer** — records and applies command lists.
- **CommandValidator** — runs before Apply (e.g. ensures reparent does not create a cycle, entity exists, etc.).

Transactions (Transaction, TransactionManager) group named command lists for high-level operations (e.g. “LoadLevel”, “SpawnEntity”).

---

## 5. Observation (observation/WorldObservation.h)

**WorldObservation** wraps a **WorldView** and exposes read-only access for rendering and gameplay:

- **GetTransform(entity)** — returns **world** transform (from TransformStore world TRS cache, or derived and cached from world matrix if using legacy path).
- **GetXxxView()** — zero-copy spans over component data: **GetTransformsView**, **GetRenderablesView**, **GetLightsView**, **GetCamerasView**, **GetMeshRenderersView**, **GetRigidBodiesView**, **GetCollidersView**. Data is gathered from the view’s component stores (and for transforms, from TransformStore); valid until next view mutation or next GetXxxView call (depending on implementation).
- **GetTransformSoAView()** — SoA view over transform entities, positions, rotations, scales (world or local as documented).
- **GetRegionsContainingPoint**, **GetHighestPriorityRegion** — spatial queries.

Observation does not mutate state; it is used by the render bridge, gameplay code, and editors to read the current world.

---

## 6. Serialization (Serialization/)

- **WorldSerializer** — save/load **WorldState** (or equivalent) to/from JSON: entities, component data, hierarchy (parent refs), metadata, tags.
- **WorldSerializationTraits** — per-component **Serialize** / **Deserialize** (e.g. Transform with type, color, intensity for Light; innerCone/outerCone; etc.).
- **ComponentSerializer** — type-key to component name and (de)serialization.
- **WorldSnapshot** — optional immutable snapshot type for save/restore or networking.

Transform component (and any TRS in serialized form) is read from JSON and then written into the store via Add/SetTransform, so sanitization applies on load as well.

---

## 7. API Layer (api/world)

The public **api/world** layer (not under eyelib/world) exposes:

- **World** — CreateEntity, DestroyEntity, AddComponent, RemoveComponent, SetTransform, SetParent (with preserveWorldTransform), GetParent, GetChildren, LoadScene, SaveScene, **CaptureRenderSnapshot**, **SetFrameSnapshot**, **SetCaptureSnapshotCallback**, **GetFrameSnapshot**.
- **Observation** — wraps world observation for API consumers.
- **RenderSnapshot** — immutable snapshot (renderables, mesh renderers, cameras, lights, transforms, rigid bodies, colliders) filled by **CaptureRenderSnapshot** and used by the render bridge to build the scene without holding a reference to the live world.

CaptureRenderSnapshot is called once per frame (or when building the scene); it fills a **RenderSnapshot** from the current world state (including world transforms from TransformStore). The render bridge then uses this snapshot to build drawables and light arrays.

**SetCaptureSnapshotCallback** — For **GameRuntime** / **WorldRenderBridge** play sessions, binding is centralized: `WorldRenderBridge::BindWorldSnapshotCapture` after bridge init and `UnbindWorldSnapshotCapture` before teardown (`GameRuntime::Shutdown`). Do not register the callback from `BuildScene` or duplicate clears in editor/player hosts.

---

## 8. Conventions

- **Transforms** — Local TRS is authoritative. World TRS is cached; matrices are derived in **Compose** and in **SetWorldTRSByIndex**. All writes go through sanitization (NaN, normalize quat, clamp scale).
- **Hierarchy** — Use **SetParent** / **RemoveParent** via commands; validate with **WouldCreateCycle** before reparent. **MarkSubtreeDirty** after hierarchy change.
- **Commands** — Mutations go through commands; commands call **MarkLocalDirty** or **MarkSubtreeDirty** when they change transform or hierarchy.
- **Observation** — Read-only; use **WorldObservation** or **WorldView** for queries and view building; do not mutate state from observation paths.
- **TRS math** — Use **ComposeTRS** / **InverseTRS** for hierarchy and reparent; use **Compose** only when a matrix is required (e.g. rendering).

---

*Last updated to reflect TRS-based transform system, world TRS cache, algebraic ComposeTRS/InverseTRS, reparent without Decompose, and NaN/scale/quat safety on all transform writes.*
