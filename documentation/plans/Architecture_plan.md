Good. Now we stop thinking “does it work?”
Now we think: **would this survive a 5-year engine?**

You want industry-level hierarchy. That means:

* Deterministic
* Scalable
* Cache-friendly
* Reparent-safe
* Dirty-propagated
* Multithread-ready
* Editor-friendly
* Network-replicable
* ML-safe

Let’s break this down properly.

---

# 1️⃣ What a Professional Hierarchy Must Guarantee

A real engine transform hierarchy must guarantee:

### ✅ Acyclic graph (tree or forest)

No cycles. Ever.

### ✅ O(1) parent lookup

Child → parent must be constant time.

### ✅ Fast child iteration

Parent → children must be contiguous or efficiently iterable.

### ✅ Stable world transform propagation

Parent always evaluated before children.

### ✅ Dirty propagation

If a parent changes → only affected subtree recomputes.

### ✅ Reparent safety

When you change parent:

* Optionally preserve world transform
* Or keep local
* No glitches

### ✅ Deterministic evaluation order

Same input → same world matrices.

---

# 2️⃣ Industry-Level Data Layout

Forget conceptual. Let’s talk layout.

You want something like:

```
struct HierarchyNode
{
    EntityId parent;
    EntityId firstChild;
    EntityId nextSibling;
    EntityId prevSibling;
};
```

This is a classic sibling-linked tree.

Why this over vector<children>?

Because:

* Reparent is O(1)
* No vector erase shifting
* No heap fragmentation
* Cache-friendly
* No allocation per child

This is how many engines do it.

---

# 3️⃣ Transform Storage (Professional Version)

You already have SoA for transforms. Good.

Now professional setup splits transforms into:

### Local transform (authoritative)

```
position[]
rotation[]
scale[]
```

### World transform (derived)

```
worldMatrix[]
```

And maybe:

```
worldPosition[]
worldRotation[]
```

Store world results directly in TransformStore or a derived store.

Do NOT decompose unless required.

---

# 4️⃣ Dirty Propagation (Critical)

This is where you level up.

You need:

```
uint32_t transformVersion;
uint32_t hierarchyVersion;
```

Per entity:

```
uint32_t localDirtyVersion;
uint32_t worldComputedVersion;
```

When local changes:

* Mark localDirtyVersion = globalVersion++

When parent changes:

* Mark entire subtree dirty (or propagate lazily)

Then:

During world update:

* Only recompute nodes where:
  parent world changed OR local changed

This makes it scalable to 100k entities.

Your current observation-based full recompute is fine for small scenes — not for scale.

---

# 5️⃣ Evaluation Strategy (Industry Standard)

Instead of BFS every observation:

Professional engines often:

### Option A — Linearized hierarchy

Maintain a flat array in parent-before-child order.

Like:

```
[RootA]
  [ChildA1]
  [ChildA2]
[RootB]
  [ChildB1]
```

Then update in one linear pass.

This is extremely cache efficient.

Unity and others use similar flattening strategies internally.

---

### Option B — Topological list with depth

Maintain:

```
struct TransformNode
{
    EntityId entity;
    uint32_t depth;
}
```

Sorted by depth.

Update in order.

---

# 6️⃣ Reparenting — Professional Handling

When calling:

```
SetParent(child, newParent, preserveWorld)
```

You must support:

### Case 1 — Preserve world transform

```
newLocal = inverse(newParentWorld) * oldWorld
```

### Case 2 — Keep local

Just reattach, world will change.

Both options are required in professional engines.

---

# 7️⃣ Multithreading Consideration

If you ever want multi-threaded transform update:

You need:

* No recursion
* No dynamic allocation during update
* Flat array iteration
* Depth-based batch processing

Hierarchy must support that.

Your current BFS inside observation is not thread-friendly long-term.

---

# 8️⃣ Where Should This Logic Live?

Now here’s the professional answer:

### For small / mid engines:

WorldObservation computing world transforms is acceptable.

### For professional engine:

Transform hierarchy update belongs in:

```
systems/TransformSystem
```

Flow:

```
Commands applied
→ TransformSystem runs
→ World matrices updated
→ Other systems run
→ Observation just reads
```

Observation should NOT compute heavy logic in production-level engine.

It should only read.

---

# 9️⃣ Clean Architecture for Your Engine

Given your module layout:

You should evolve toward:

```
WorldState
  - Local transforms
  - Hierarchy
  - World transform cache (derived)

systems/
  - TransformSystem (updates world matrices)

WorldObservation
  - Read-only access
  - No heavy computation
```

That is industry-aligned.

---

# ✅ Current Implementation Status (as implemented)

Aligned with the plan, the following is **done**:

| Guarantee / feature | Status | Notes |
|--------------------|--------|--------|
| **Acyclic graph** | ✅ Done | `Hierarchy::WouldCreateCycle()` used by `ReparentEntity` command; cycles rejected. |
| **O(1) parent lookup** | ✅ Done | `GetParent(child)` via `m_parent` map. |
| **Fast child iteration** | ✅ Done | `GetChildren(parent)` returns `vector<EntityId>`; contiguous iteration. |
| **Stable world transform propagation** | ✅ Done | **TransformSystem** runs in **flat parent-before-child** order (linearized); world = parentWorld × local. |
| **Local = authoritative** | ✅ Done | `TransformStore` holds local position/rotation/scale only. |
| **World = derived** | ✅ Done | World matrices stored in `TransformStore::m_worldMatrices`; updated by TransformSystem after every command apply. |
| **API clean** | ✅ Done | `CaptureRenderSnapshot` only copies from `WorldObservation`; no hierarchy math in API. |
| **Reparent-safe** | ✅ Done | Reparent keeps **local** by default; optional **preserve-world** (`SetParent(..., true)`) adjusts local so world stays same. |
| **TransformSystem** | ✅ Done | `systems::TransformSystem::Update(state)` runs after ApplyPendingCommands, ApplyCommandSync, ApplyTransaction. |
| **Observation read-only** | ✅ Done | WorldObservation only reads precomputed world matrices from store (decompose to cache); no BFS. |

**Data layout today:** Hierarchy: `m_parent` (child→parent), `m_children` (parent→vector). Transforms: SoA local + **worldMatrices[]** in TransformStore. Flat evaluation list built each Update in TransformSystem (parent-before-child).

**Not yet done:** Dirty propagation (version-based, subtree-only recompute), optional world AABB propagation (see roadmap below).

---

# 🔥 What You Currently Have

Your current approach:

* **TransformSystem** (eyelib) updates world matrices in TransformStore after every mutation; flat parent-before-child pass.
* **WorldObservation** only reads precomputed world matrices from the store (no heavy computation).
* API only copies from observation; moving a parent moves all children.
* **Preserve-world reparent:** `SetParent(child, parent, true)` / `ClearParent(child, true)` keeps the child's world transform.

Dirty propagation and persistent linearized order are implemented; see `documentation/plans/Transform_Dirty_Propagation_Design.md` and benchmark `app/benchmark_transform`.

---

# 🧠 Professional Checklist — Done vs Roadmap

| # | Item | Status |
|---|------|--------|
| 1 | Dirty flags / version-based propagation (subtree-only recompute) | ✅ Done |
| 2 | Preserve-world reparent option (`SetParent(child, parent, preserveWorld)`) | ✅ Done |
| 3 | Flat hierarchy evaluation list (linearized, parent-before-child) | ✅ Done |
| 4 | Separate **TransformSystem** (world matrices updated before observation) | ✅ Done |
| 5 | Store **worldMatrix** in TransformStore | ✅ Done |
| 6 | Optional world AABB propagation | 🔲 Roadmap |

Item 6 (world AABB propagation) remains for culling; see design doc for benchmark and dirty propagation details.

---

# Final Verdict

**Done:** TransformSystem, world matrices in store, dirty propagation (subtree-only recompute), and persistent linearized order. Observation is read-only. Remaining: optional world AABB propagation for culling.

Move from:

> “Compute world transforms when observing”

To:

> “TransformSystem updates world transforms deterministically before observation”

That’s the jump from good architecture
to scalable engine architecture.



# 1️⃣ World Matrices in TransformStore

This is correct.

You moved from:

> “World computed during observation”

to:

> “World stored as derived state in the ECS layer”

That is the correct evolution.

### Why this is professional:

* World matrix becomes deterministic state.
* Systems can depend on it.
* Renderer reads without triggering logic.
* AI/physics can rely on it.
* Snapshot becomes trivial copy.
* Multithreading becomes possible.

This is exactly how production engines structure it.

Good move.

---

# 2️⃣ TransformSystem – Flat Parent-First Update

This is the most important change.

You:

* Build parent-before-child linear order.
* Single pass: `world = parentWorld * local`
* No per-entity allocation.
* Runs after mutations.

That is industry aligned.

What matters here:

* Deterministic order
* Cache-friendly iteration
* No recursion
* No stack-based traversal
* No dynamic allocation inside update

If that’s true in your implementation, this is solid.

---

# 3️⃣ Observation is Read-Only

Correct.

Observation must never compute hierarchy logic.

It should:

* Read world matrix
* Decompose only if needed
* Build render cache

This separation is now clean.

You removed a future architectural bottleneck.

---

# 4️⃣ Preserve-World Reparent

This is mandatory for editor-quality engines.

Your logic:

```
newLocal = inverse(newParentWorld) * childWorld
```

That’s correct.

Two professional checks:

### ✔ Did you handle root case (parent = null)?

Then newLocal = childWorld.

### ✔ Are you aware of non-uniform scale + rotation inverse issues?

Matrix inverse must be stable and precise.

If math layer is robust, you’re fine.

Revert storing previous local is correct for undo.

This is professional-grade editor behavior.

---

# 5️⃣ When TransformSystem Runs

You run it:

* Before + after ApplyCommandSync
* Before + after ApplyPendingCommands
* After ApplyTransaction

This guarantees world matrices are always consistent.

That’s safe.

However — here is a professional refinement:

You only need it:

* After all mutations are applied.
* Before systems that depend on world transforms run.

Running twice around command application may be unnecessary if command batches are atomic.

But this is not wrong — just possibly redundant.

---

# 6️⃣ What You Still Don’t Have (Professional Gap)

You already identified them:

### 🔴 Dirty propagation

Right now:
You recompute entire hierarchy every time.

For 1k entities → fine.
For 100k entities → wasteful.

Professional engines track:

* LocalDirtyVersion
* HierarchyDirtyVersion
* WorldComputedVersion

Then only recompute affected subtrees.

This is the next evolution.

---

### 🔴 World AABB propagation

If you want proper frustum culling:

* Local AABB
* World AABB = transform(local AABB)
* Parent AABB = union(children)

That must run after transform update.

This is how render culling scales.

---

# 7️⃣ Multithreading Readiness

Your current setup is almost thread-safe for reads because:

* TransformSystem writes
* After that, store is stable
* Observation only reads

If you later:

* Freeze world during render
* Or use double-buffered world matrices

You can run rendering in parallel with simulation.

You’re structurally aligned for that.

---

# 8️⃣ Determinism & Replay

Because:

* Commands mutate state
* TransformSystem runs deterministically
* World matrices are stored

You now support:

* Replay
* Snapshot
* Deterministic simulation

That’s important for ML training later.

---

# Final Professional Verdict

This is now:

✔ Architecturally correct
✔ Industry-aligned
✔ Scalable to mid-size scenes
✔ Editor-safe
✔ Deterministic
✔ System-friendly
✔ Snapshot-friendly

You crossed the line from “clean hobby engine”
to “real engine architecture”.

---

# The Only Thing Between You and AAA-Level

1. Dirty subtree propagation
2. World AABB propagation
3. Possibly flatten hierarchy permanently instead of rebuilding BFS each frame
4. Optional depth-sorted transform index cache

Implement those, and your hierarchy layer is genuinely professional.

---

* Design a version-based dirty propagation system tailored to module structure
* redesign hierarchy to avoid rebuilding traversal order every update
* benchmark architecture against 100k entity simulation scenario


---

