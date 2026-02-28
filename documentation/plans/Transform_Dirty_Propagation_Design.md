# Transform Dirty Propagation & Persistent Linearized Hierarchy

Design for version-based dirty propagation and persistent parent-before-child order, tailored to the OpenEye world module (TransformStore, Hierarchy, TransformSystem).

---

## 1. Goals

* **Dirty propagation:** Only recompute world matrices for entities whose local transform or ancestor changed. Scale to 100k+ entities when only a small subtree is modified.
* **No BFS per frame:** Maintain a persistent linearized order (parent-before-child); rebuild only when hierarchy or transform set changes.
* **Deterministic, cache-friendly:** Single linear pass over the flat array; no per-entity allocation during update.

---

## 2. Version-Based Dirty Model

### 2.1 Per-entity versions (in TransformStore)

Add two SoA arrays, same size and order as `m_entityIds`:

```cpp
std::vector<uint32_t> m_localDirtyVersion;   // bumped when local transform is set
std::vector<uint32_t> m_worldComputedVersion; // set when world matrix was computed
```

* **localDirtyVersion:** Incremented whenever this entity’s local transform is written (SetTransform, or preserve-world reparent writing new local). Identifies “this local data has changed since last world update.”
* **worldComputedVersion:** When we compute world for this entity, set `worldComputedVersion[i] = localDirtyVersion[i]`. So “world is valid for this local version.”

### 2.2 When to mark dirty

| Event | Action |
|-------|--------|
| **SetTransform(entity)** | `localDirtyVersion[index] = ++globalVersion` (or entity-local bump). |
| **Reparent(child, …)** | Mark **child and all descendants** (BFS from child) as dirty: set each `localDirtyVersion[i] = ++globalVersion`. So any reparent invalidates the subtree’s world matrices. |
| **Add(entity)** | New entity: `localDirtyVersion[i] = 0`, `worldComputedVersion[i] = invalid` (e.g. 0). First Update will compute. |
| **Remove(entity)** | No version update needed (entity gone). |

We need a **global version** only to generate new dirty stamps when we mark a subtree (reparent). So:

* **Option A – Global version:** One `uint32_t m_transformVersion` in WorldState or TransformSystem; increment on any local or hierarchy change; assign to `localDirtyVersion` when marking dirty. Simple.
* **Option B – Per-entity stamp only:** When marking dirty, set `localDirtyVersion[i] = worldComputedVersion[i] + 1` or use a simple “dirty flag” and avoid global. For subtree reparent we still need to walk descendants and set each entity’s localDirtyVersion to a value strictly greater than its worldComputedVersion. E.g. set each to `max(localDirtyVersion, worldComputedVersion) + 1` or use one global “generation” that we assign when marking.

We use **one global version** in TransformSystem (or WorldState): `m_globalTransformVersion`. When we need to mark entity or subtree dirty we set `localDirtyVersion[i] = ++m_globalTransformVersion`. Then “need recompute” is `localDirtyVersion[i] != worldComputedVersion[i]`.

### 2.3 Recompute condition (TransformSystem::Update)

Process entities in **linearized order** (parent before children). For each entity `i`:

* **parentUpdated:** Parent has transform and was updated this pass (see below).
* **needUpdate = (localDirtyVersion[i] != worldComputedVersion[i]) || parentUpdated.**

If `needUpdate`: compute `world = parentWorld * local`, write world matrix, set `worldComputedVersion[i] = localDirtyVersion[i]`, and mark this entity as “updated this pass.”

We need “updated this pass” for the parent check. Use a temporary buffer owned by TransformSystem:

```cpp
mutable std::vector<uint8_t> m_updatedThisPass; // 1 per entity, by store index
```

At start of Update: `m_updatedThisPass.assign(store.Size(), 0)`. When we compute entity at index `i`, set `m_updatedThisPass[i] = 1`. Then `parentUpdated = (parent != Invalid && store.Has(parent) && m_updatedThisPass[parentIndex])`.

### 2.4 Who bumps dirty

* **SetTransformCommand::Apply:** After writing local transform, call `state.GetTransformStore().MarkLocalDirty(entity)` (or TransformSystem::MarkDirty(entity)).
* **ReparentEntityCommand::Apply:** After changing hierarchy, call `TransformSystem::MarkSubtreeDirty(state, child)` so that child and all descendants get `localDirtyVersion = ++globalVersion`.

So TransformSystem must expose `MarkLocalDirty(EntityId)` and `MarkSubtreeDirty(WorldState&, EntityId)`. That requires TransformSystem to have access to TransformStore (it already does via state) and to the list of transform entities + hierarchy to walk the subtree. MarkSubtreeDirty walks descendants via GetChildren and only marks entities that have a transform.

Alternatively, dirty marking can live in the commands: they already have state. So we could add `TransformStore::MarkLocalDirty(entity)` and a free function or Hierarchy + TransformStore helper `MarkSubtreeDirty(store, hierarchy, root)` that walks subtree and marks each entity present in the store. Then TransformSystem only reads the version arrays and doesn’t need to own the “global version” for marking—but we still need a global version to assign. So either:

* **TransformStore** holds `m_globalTransformVersion` and provides `MarkLocalDirty(entity)`, `MarkSubtreeDirty(hierarchy, root)` (store knows entity set; hierarchy provides children). Commands call these. TransformSystem only runs the update pass.  
* **TransformSystem** holds `m_globalTransformVersion` and provides `MarkLocalDirty(state, entity)`, `MarkSubtreeDirty(state, root)`. Commands need access to TransformSystem—possible if World holds the system and commands get it via state or a callback. More coupled.

Cleanest: **TransformStore** owns `m_globalTransformVersion`, `m_localDirtyVersion`, `m_worldComputedVersion`, and methods `MarkLocalDirty(entity)`, `MarkSubtreeDirty(hierarchy, root)`. TransformSystem only reads these and writes world matrices + worldComputedVersion. Commands call `state.GetTransformStore().MarkLocalDirty(entity)` etc. No extra coupling.

---

## 3. Persistent Linearized Order

### 3.1 Goal

Avoid building a new `vector<EntityId>` BFS order every Update. Keep a single **linearized list** (parent-before-child) and only rebuild when:

* The set of entities with transforms changes (Add/Remove transform), or  
* The hierarchy structure changes (SetParent / RemoveParent).

### 3.2 Ownership and invalidation

**Option A – TransformSystem owns the list**  
TransformSystem holds `m_linearizedOrder: vector<EntityId>` and a cache key:

* `m_lastLinearizedStoreSize` (or store version)  
* `m_lastHierarchyVersion` (hierarchy must expose a version)

When `Update(state)` runs, if `store.Size() != m_lastLinearizedStoreSize` or `hierarchy.Version() != m_lastHierarchyVersion`, rebuild `m_linearizedOrder` (one BFS), then update cache keys. Then run the single pass over `m_linearizedOrder` with dirty logic.

**Option B – WorldState owns the list**  
A small “TransformHierarchyCache” in WorldState holds the linearized list and a hierarchy version. Hierarchy bumps version on SetParent/RemoveParent; TransformStore bumps a “layout version” on Add/Remove/Clear. Cache invalidates when either version changes. TransformSystem asks the cache for `GetLinearizedOrder(store, hierarchy)` and gets a span; cache rebuilds internally when needed.

We choose **Option A** for minimal surface: Hierarchy gets a version number; TransformSystem owns the list and rebuilds when store size or hierarchy version changes.

### 3.3 Hierarchy version

Add to Hierarchy:

```cpp
uint32_t GetVersion() const { return m_version; }
```

In `SetParent` and `RemoveParent` (and `Clear`): `++m_version`. TransformSystem stores `m_lastHierarchyVersion` and `m_lastTransformCount`. Rebuild linearized order when `hierarchy.GetVersion() != m_lastHierarchyVersion || store.Size() != m_lastTransformCount`.

### 3.4 Rebuild algorithm

Same as current BFS: collect transform entity set; enqueue roots; while queue not empty, pop entity, push to `m_linearizedOrder`, enqueue children that have transforms. No change from current logic—just run only when versions change.

---

## 4. Module Wiring

| Component | Responsibility |
|-----------|----------------|
| **TransformStore** | `m_localDirtyVersion`, `m_worldComputedVersion`, `m_globalTransformVersion`; `MarkLocalDirty(entity)`; `MarkSubtreeDirty(hierarchy, root)`; bump versions in Add/Remove/Clear as needed. |
| **Hierarchy** | `m_version`; bump in SetParent, RemoveParent, Clear. |
| **SetTransformCommand** | After Apply, call `state.GetTransformStore().MarkLocalDirty(entity)`. |
| **ReparentEntityCommand** | After Apply, call `state.GetTransformStore().MarkSubtreeDirty(state.GraphHierarchy(), child)`. |
| **TransformSystem** | Owns `m_linearizedOrder`, `m_lastHierarchyVersion`, `m_lastTransformCount`, `m_updatedThisPass`. Update(): if cache stale, rebuild linearized order; then one pass over it with dirty checks and world computation. |

---

## 5. Benchmark (100k entities)

* **Setup:** Create a deep or wide hierarchy (e.g. 100k entities in a chain or balanced tree), each with a Transform component.  
* **Measure:** Time for `TransformSystem::Update(state)` (and optionally CaptureRenderSnapshot).  
* **Scenarios:**  
  (1) Full recompute every frame (no dirty).  
  (2) Dirty propagation: change one leaf’s local each frame; measure Update time (should be O(subtree size) or O(depth) with subtree propagation).  
* **Executable:** `app/benchmark_transform` (target `BenchmarkTransform`). Default 10k entities; set `N = 100000u` in source for 100k. Run from `build/bin/Debug` or `Release`.
* **Target:** Document baseline for 100k full update in Release (e.g. &lt; 10 ms) and show steady-state and single-leaf dirty are cheaper.

---

## 6. Summary

* **Dirty:** Per-entity `localDirtyVersion` / `worldComputedVersion` in TransformStore; global version for marking; MarkLocalDirty on SetTransform, MarkSubtreeDirty on Reparent; TransformSystem only recomputes when `localDirtyVersion != worldComputedVersion` or parent was updated this pass.  
* **Persistent order:** TransformSystem keeps a linearized parent-before-child list; rebuild only when hierarchy version or transform count changes.  
* **Benchmark:** 100k-entity hierarchy + Update timing and optional snapshot; compare full vs dirty update.

This keeps the existing “world in store + TransformSystem + read-only observation” design and adds subtree-scoped updates and zero-allocation steady-state updates.
