# Runtime Efficiency — Unreal-Level Plan

This document is a **comprehensive plan** to evolve the OpenEye runtime hot path and architecture toward **Unreal/Unity-level efficiency and capability**. It covers where we are today, the gaps vs. professional engines, a phased roadmap with clear priorities, code touch points, metrics, and risks.

**One-line summary:** Same architecture (one state, one read path, one write path); add zero-copy reads, cache-friendly layout, parallelism, and low-allocation memory so the runtime **performs** like a professional engine.

---

## 1. Goal

**Target:** Make the runtime hot path (per-frame simulation, rendering, and systems) as **efficient and powerful** as engines like Unreal and Unity: minimal allocations, cache-friendly data layout, scalable parallelism, and professional read/write patterns.

**Scope:** Runtime player and engine core (world state, observation, systems, rendering pipeline). Editor and tooling are out of scope except where they affect runtime design.

**Non-goals:** We are not cloning Unreal feature-for-feature; we are adopting the **patterns and level of optimization** that make their runtimes scalable and efficient.

---

## 2. Comparison at a Glance

| Area | Unreal / Unity | OpenEye Today | OpenEye Target |
|------|----------------|---------------|----------------|
| **Read path** | Views, spans, iterators over live storage; no bulk copy | `Observation::GetTransforms()` etc. return `std::vector<Entry>` → full copy every frame | Views/spans or fill caller buffer; zero copy |
| **Data layout** | SoA or archetype/chunk ECS; contiguous, cache-friendly | `ComponentStore<T>` = `unordered_map<EntityId, T>`; pointer-chasing | SoA or chunk-based; contiguous iteration |
| **Multithreading** | Job system with dependencies and sync points | Single-threaded; `ThreadPool` exists but no job graph | Job system; physics/animation/culling as jobs |
| **Memory** | Frame allocator, pools, minimal hot-path allocs | Vectors allocated per frame in Observation; standard allocator | Frame allocator; reuse buffers; pools where needed |
| **SIMD** | Vectorized bulk math (transforms, culling) | Scalar (glm) | SIMD where profiling shows gain |
| **Rendering** | Culling, batching, LOD, parallel command build | Pipeline present; no explicit culling/batching/LOD jobs | Frustum culling; batching/LOD; scalable build |

---

## 3. Current State (Where We Are)

### 3.1 What We Have (Strengths)

| Area | Status |
|------|--------|
| **World model** | WorldState = single source of truth; entities, components, hierarchy, serializable. |
| **Mutation** | One legal write path: commands + validation; LoadScene via command. |
| **Observation** | Single read path: Observation layer; no direct state access from outside. |
| **Hot path** | No JSON/YAML/file I/O in the frame loop; WorldState is the in-memory representation. |
| **Runtime systems** | Physics, animation, rendering, audio, scripting wired; scheduler with fixed/variable timestep. |
| **Serialization** | One canonical path: WorldState + WorldSerializer; Scene/BinarySerializer deprecated for scenes. |

So: **architecture is clean** (one state, one read path, one write path). The hot path is **correct** and **format-agnostic** (no file format in the loop).

### 3.2 Known Gaps (Efficiency & Scale)

| Gap | Current | Impact |
|-----|---------|--------|
| **Read path copies** | Observation getters return `std::vector<Entry>` → full copy every frame (transforms, renderables, lights, etc.). Implemented in `api/world/Observation.cpp` by iterating `Components<T>().All()` and pushing into a new vector. | Allocations + copies every frame; O(n) per system; allocator pressure. |
| **Data layout** | `ComponentStore<T>` = `std::unordered_map<EntityId, T, EntityIdHash>` per type (see `eyelib/world/ComponentStorage/ComponentStore.h`). | Pointer-chasing, cache-unfriendly iteration; no SoA/chunk layout. |
| **Single-threaded** | Physics, animation, render, scripting run on main thread in sequence. | No parallelism; underutilized CPU. |
| **Memory** | Standard allocators; per-frame vectors in Observation; no frame/pool strategy in hot path. | Allocator pressure; potential fragmentation. |
| **SIMD** | Math is scalar (glm, etc.). | No vectorized hot loops (e.g. transform bulk update). |
| **Rendering** | Modern pipeline (deferred/forward/forward+) but no explicit job/culling/batching strategy. | Could scale better with more objects and draw calls. |

### 3.3 Key Code Locations (Today)

| Concern | Files / Types |
|--------|----------------|
| Observation (read path) | `api/world/Observation.h`, `api/world/Observation.cpp`; `eyelib/world/observation/WorldObservation.h` |
| Component storage | `eyelib/world/WorldState.h`, `eyelib/world/ComponentStorage/ComponentStore.h`, `IComponentStore.h` |
| Thread pool | `eyelib/core/job/ThreadPool.h`, `ThreadPool.cpp` (Submit, SubmitBatch, Wait; no job graph) |
| Hot-path callers | `app/player/WorldRender.cpp`, `app/player/WorldPhysics.cpp`, `app/player/WorldAudio.cpp`, `app/player/scripting/WorldLuaBindings.cpp` |

---

## 4. Gap Analysis vs. Unreal / Professional Engines

How Unreal (and similar engines) handle the same areas:

| Area | Unreal / Pro Pattern | Our Gap |
|------|----------------------|--------|
| **Read path** | Views, spans, or iterator ranges over live storage; no bulk copy. Callers iterate; data stays in place. | We copy full vectors every frame. |
| **Data layout** | SoA (Structure of Arrays) or archetype/chunk ECS: contiguous arrays per component, cache-friendly iteration. | We use map per type; iteration is pointer-chasing. |
| **Multithreading** | Job system: work broken into jobs, run on worker threads; dependencies and sync points. | We are single-threaded. |
| **Memory** | Frame allocators, pools, minimal allocations in hot path; scratch buffers reused. | We allocate vectors per frame; no frame allocator. |
| **SIMD** | Vectorized math (SSE/AVX) for bulk ops (transforms, bounds, culling). | Scalar only. |
| **Rendering** | Culling, LOD, batching, instancing, parallel command list build; GPU-driven where applicable. | We have pipeline but no job-based culling/batching. |

**Summary:** We have the **right architecture** (one state, one read path, one write path). We are missing the **efficiency layer**: zero-copy reads, cache-friendly layout, parallelism, and low-allocation memory strategy.

---

## 5. Phased Plan

### Phase 1: Zero-Copy Read Path (Views / Spans)

**Goal:** Observation returns **views** (or fill caller-provided buffers) instead of new vectors. No per-frame copy of component data.

**Work:**

1. **Define view types**
   - e.g. `std::span<const TransformEntry>` or a small `ObservationView<T>` (pointer + size).
   - Lifetime rule: view is valid only for the current frame / until world is mutated.

2. **Change Observation API**
   - Add `GetTransformsView()` (and similar) returning `span` over internal storage, **or**
   - Add `GetTransformsInto(std::vector<…>& out)` to fill a caller-provided buffer (reuse each frame).
   - Keep existing `GetTransforms()` returning vector if needed for compatibility during migration; deprecate or remove once callers use views.

3. **Expose storage in a view-safe way**
   - WorldObservation/WorldView must expose read-only access to component stores such that we can form a span (e.g. over the values of the map, or over an internal contiguous buffer if we change layout later).
   - If current store is `unordered_map`, we cannot form a stable pointer range over map values (iteration order and pointer stability vary). So either: (a) offer **iteration** (begin/end) so callers iterate in place without a vector, or (b) have callers pass a buffer and we fill it once per frame (reuse same buffer). Option (b) avoids per-frame allocation and is a good first step.

4. **Update all hot-path callers**
   - WorldRender (BuildScene), WorldPhysics, WorldAudio, Lua batch getters: use the new view or fill-buffer API.
   - Ensure no view is stored across frame or across mutation.

**Code touch points:**

- `api/world/Observation.h`, `Observation.cpp` — add view or fill-buffer API.
- `eyelib/world/observation/WorldObservation.h` — expose iteration or buffer-fill over component data.
- `app/player/WorldRender.cpp`, `WorldPhysics.cpp`, `WorldAudio.cpp`, `app/player/scripting/WorldLuaBindings.cpp` — switch to view or fill-buffer.

**Risks & mitigations:**

- **Risk:** View invalidated by mutation. **Mitigation:** Document lifetime (frame scope); no mutation during read phase; or use fill-buffer so caller owns memory.
- **Risk:** Map storage doesn’t give contiguous pointer range. **Mitigation:** Prefer fill-buffer API for Phase 1; Phase 2 (SoA) will allow true spans.

**Outcome:** Hot path no longer allocates and copies full transform/renderable/light arrays every frame. **Pattern matches Unreal/Unity** for read path.

**Priority:** High. Foundation for everything else; low risk.

---

### Phase 2: Cache-Friendly Data Layout (SoA / Chunks)

**Goal:** Component data stored so that iteration is **contiguous** and **cache-friendly** (SoA or archetype/chunk style). Enables fast bulk iteration and, later, SIMD and jobs.

**Work:**

1. **Design**
   - **Option A — SoA per component type:** e.g. `TransformStore` = separate arrays for position[], rotation[], scale[] (or similar). One index = one entity; entity index → component.
   - **Option B — Archetype/chunk:** Entities grouped by “component set”; each chunk is a fixed-size block of entities with same components; chunk = contiguous SoA for that set. (Larger refactor.)
   - Start with Option A for 1–2 critical types (e.g. Transform) to prove the pattern.

2. **Implement**
   - New store type(s) with contiguous storage; entity → index mapping (e.g. `EntityId` → dense index; reverse map for lookup).
   - Migrate one component (e.g. Transform) from map to new store; keep same public API (Add, Remove, Get, iteration) where possible.
   - Observation returns span over the contiguous arrays (or over a “view” struct that spans multiple arrays).

3. **Expand**
   - Migrate other hot components (Renderable, Light, Camera, etc.) to the new layout.
   - Deprecate or remove old map-based store for those types.

**Code touch points:**

- `eyelib/world/ComponentStorage/` — new SoA store type(s); `WorldState` registration.
- `eyelib/world/WorldState.h` — store type registration (template or type-erased).
- `eyelib/world/observation/WorldObservation.h` — return spans over SoA arrays.
- Component types: `eyelib/world/components/Transform.h`, etc.

**Risks & mitigations:**

- **Risk:** Large refactor; entity ID semantics (stable index vs. id). **Mitigation:** Start with Transform only; keep EntityId; maintain entity→index map.
- **Risk:** Serialization must read/write new layout. **Mitigation:** WorldSerializer already per-component; adapt serialization for new store format.

**Outcome:** Iteration over transforms (and other migrated components) is contiguous and cache-friendly. **Foundation for SIMD and parallel jobs.**

**Priority:** High, after Phase 1. Depends on Phase 1 for view API (so we don’t expose map iterators that become invalid after layout change).

---

### Phase 3: Job System & Parallelism

**Goal:** Run independent work (physics, animation, culling, render scene build) in **parallel** where safe. Use a **job system** (thread pool + job graph) so the main thread only syncs, not does all work.

**Work:**

1. **Job system**
   - Extend `eyelib/core/job/ThreadPool`: add **job** abstraction — enqueue jobs, dependencies (job B runs after job A), wait for completion.
   - Jobs are small units of work (e.g. “update these 100 transforms”, “build draw list for this chunk”).

2. **Identify parallelizable work**
   - Animation: per-skeleton/clip work can be parallel.
   - Physics: simulation can run on a job; sync results before render.
   - Culling / visibility: per-chunk or per-entity.
   - Render scene build: build draw lists in parallel from read-only world view.

3. **Rules**
   - Only **read** WorldState (or Observation) from jobs during “read phase”; **writes** (commands) stay on main thread and apply after jobs complete.
   - No mutation of world from jobs; use double-buffer or result buffers that are merged on main thread if needed.

4. **Integrate**
   - Scheduler: “read phase” (build views) → enqueue jobs (physics, animation, culling, etc.) → wait → “write phase” (apply commands, sync physics to world) → render (single-threaded or with render thread).

**Code touch points:**

- `eyelib/core/job/ThreadPool.h`, `ThreadPool.cpp` — job graph, dependencies, completion wait.
- `eyelib/world/World.cpp` (or scheduler) — frame phases: read → jobs → sync → write → render.
- `app/player/WorldRender.cpp`, `WorldPhysics.cpp`, animation systems — split into job-sized work.

**Risks & mitigations:**

- **Risk:** Data races if world is read while written. **Mitigation:** Strict phases; no mutation in jobs; sync before any write.
- **Risk:** Overhead of job dispatch outweighs benefit for small scenes. **Mitigation:** Minimum batch size; single-threaded path for small entity counts.

**Outcome:** Multi-core utilization; frame time reduced for CPU-bound scenes. **Matches Unreal-style parallelism.**

**Priority:** Medium–high, after Phase 2. SoA/chunk layout makes it easier to split work (e.g. per-chunk jobs).

---

### Phase 3b: Render Thread & Command List (Unreal-Level Multithreading)

**Goal:** Separate **game thread** from **render thread** so the game can prepare the next frame while the render thread draws the current one. Optionally introduce a **command-list** style so the game thread never touches the graphics API; only the render thread (or a dedicated RHI thread) does. This matches Unreal’s model: Game Thread → Render Thread → (optional) RHI Thread.

**Current state:** One thread does everything: game logic, physics sync, BuildScene, `Render()`, BlitToScreen. All OpenGL calls happen on that thread.

**What changes:**

1. **Double- or triple-buffer render input**
   - Game thread: builds **scene data** (or a **command list**) for frame N into buffer A.
   - Render thread: reads buffer B (frame N−1), executes render (GL calls), then swaps.
   - Sync: game thread only swaps/writes when render thread has finished reading; render thread only reads after swap. Use atomic indices or two/three buffers with clear ownership.

2. **Render thread loop**
   - Dedicated thread (or use existing thread pool with a “render job” that never completes): loop { wait for new scene/commands → execute RenderPipeline::Render (or execute command list) → signal done }.
   - Game thread: BuildScene → fill buffer → signal “ready” → (optional: do other work) → wait for “render done” before reusing buffer or start next frame.

3. **No GL on game thread**
   - All GL calls (including pipeline state, draw calls, framebuffer binds) must happen on the render thread. So: **RenderPipeline** (or a thin wrapper) must only be called from the render thread. Game thread produces **data** (scene: drawables, lights, camera); render thread consumes that data and calls the pipeline.
   - Today: `WorldRenderBridge::Render()` calls BuildScene then `m_pipeline.Render(scene, …)`. After change: game thread calls BuildScene and enqueues “render this scene”; render thread dequeues and calls `m_pipeline.Render(scene, …)`.

4. **Command list (optional but Unreal-like)**
   - Instead of passing a full `RenderScene` and having the render thread call `Render(scene)` directly, game thread could produce a **list of commands** (e.g. “set viewport”, “draw mesh”, “draw fullscreen pass”). Render thread executes the list. This adds abstraction and can help with batching and reordering later, but is a larger refactor. **Minimum viable:** game thread fills a `RenderScene` in a shared buffer; render thread reads it and calls existing `Render(scene)`. No command list required for “render thread” separation.

5. **Where it lives**
   - **Player / app:** `PlayerApp` or `WorldRenderBridge` owns the sync (buffers, signals). Game thread: update world, physics sync, BuildScene into buffer, enqueue “render buffer index N”. Render thread: dequeue, call `pipeline.Render(scene_from_buffer_N)`, BlitToScreen, signal done.
   - **Pipeline:** `RenderPipeline` stays as-is; it is just **only ever called from the render thread**. No GL context on game thread.
   - **OpenGL:** GL context must be created on the render thread (or made current only on render thread). Many platforms allow one context per thread; game thread has no context.

**Benefits:**

| Benefit | Explanation |
|--------|-------------|
| **Overlap** | Game thread can run frame N+1 (e.g. physics, animation, BuildScene) while render thread draws frame N. Reduces total frame time when both are busy. |
| **Smoother pacing** | Render thread can run at display refresh rate; game thread can tick at fixed step; sync points (e.g. “wait for render done”) keep them aligned. |
| **Scalability** | Matches Unreal/AAA pattern; easier to add RHI thread later (render thread builds command list; RHI thread executes it on GPU). |
| **No GL on game thread** | Avoids driver quirks and makes it clear where GPU work is scheduled; simplifies debugging. |

**Level of change:**

| Area | Change level | What to do |
|------|----------------|------------|
| **App / player** | **High** | New render thread (or dedicated “render job”); 2–3 scene buffers; sync (mutex + condition or lock-free ring); game thread must not call any GL or `Render()` directly. |
| **WorldRenderBridge** | **Medium–High** | Split: `BuildScene()` stays on game thread and writes to a buffer; “Submit for render” enqueues buffer index. Render thread runs “draw buffer N” which calls `m_pipeline.Render(scene, …)` and BlitToScreen. |
| **RenderPipeline (eyelib)** | **Low** | No change to pipeline logic; it is just called only from render thread. Optionally: pipeline could accept a “command list” instead of immediate execution (larger change). |
| **GL context** | **Medium** | Context must be created on (or made current only on) the render thread. Platform layer (e.g. GLFW, EyelibPlatform) may need “create context for this thread” or “make current on render thread”. |
| **Memory** | **Low–Medium** | 2–3 copies of “render input” (scene data). Scene is already compact (drawables, lights, camera); double-buffering doubles that. Frame allocator: one per “frame slot” or reuse with sync. |

**Recommended order:**

1. **Minimal:** Two buffers for `RenderScene` (or equivalent). Game thread fills buffer A; when render thread is done with B, swap A↔B; render thread always reads from “current” buffer. Single mutex + condition variable.
2. **Render thread:** Create thread that runs “while running: wait for scene → Render(scene) → BlitToScreen → signal done”. Game thread: BuildScene into buffer → set “ready” → wait for “done” (or non-blocking and use last frame if not ready).
3. **GL context:** Ensure GL is only used on render thread (create/make current there). Fix any remaining GL calls on game thread (e.g. in resize or init).
4. **Optional later:** Command list abstraction; RHI thread (render thread builds commands; second thread executes them).

**Risks & mitigations:**

- **Risk:** Latency — frame N is drawn one frame behind. **Mitigation:** Acceptable for most games; or use triple buffer so game thread is never blocked.
- **Risk:** GL context threading bugs. **Mitigation:** All GL from one thread; no GL on game thread.
- **Risk:** Complexity and bugs in sync. **Mitigation:** Start with simple “game blocks until render done” (double buffer); then relax to non-blocking if needed.

**Outcome:** Game and render run in parallel; frame time and pacing align with Unreal-level multithreading. **Optional:** Command list + RHI thread for further scalability.

**Priority:** Medium (after Phase 3). Phase 3 already gives parallelism (physics, animation, culling as jobs). Phase 3b adds “render on its own thread” for overlap and clear separation; do it when you need smoother frames or are scaling toward Unreal’s full model.

---

### Phase 4: Memory Strategy (Frame Allocator, Pools)

**Goal:** **Minimal allocations in hot path**; reuse buffers; use frame allocator for short-lived data so we don’t hit the global allocator every frame.

**Work:**

1. **Frame allocator**
   - Allocator that reserves a block per frame; allocations are bump-pointer within the block; at end of frame, reset (no frees).
   - Use for temporary vectors, scratch buffers, and any short-lived data that today uses `std::vector` in the hot path.

2. **Reuse buffers**
   - BuildScene and other systems: reuse the same `std::vector` or arena-backed buffer each frame instead of creating new ones.
   - Observation “fill into buffer” API (Phase 1) fits this: caller owns one buffer, reuse every frame.

3. **Pools for hot objects**
   - Where we do allocate (e.g. drawables, light lists), consider object pools so we reuse memory instead of malloc/free per frame.

**Code touch points:**

- New: `eyelib/core/alloc/FrameAllocator.h` (or similar); integrate into frame loop.
- `app/player/WorldRender.cpp` (BuildScene) — reuse buffers; optional frame allocator for temporaries.
- Any hot path that currently allocates — audit and replace with reuse or frame allocator.

**Risks & mitigations:**

- **Risk:** Frame allocator block too small or too large. **Mitigation:** Tune block size; optional cap per frame; fallback to heap if exceeded (with warning).

**Outcome:** Low allocator pressure; stable frame times; less fragmentation. **Professional memory discipline.**

**Priority:** Medium. Can start with “reuse buffers” (Phase 1) and add frame allocator when needed.

---

#### Phase 4 Refactor: Library-Side Allocator Use (Planned)

**Context:** Phase 4 (client/API side) is done: FrameAllocator, PoolAllocator, FreeListAllocator are in the API; RenderScene can use a frame allocator; the player builds the scene with a frame allocator so scene vectors (opaque, transparent, pointLights, spotLights) use the arena. **The library (eyelib) implements LinearAllocator, PoolAllocator, FreeListAllocator but no library module uses them yet.** For Unreal-level discipline, library hot paths should also use these allocators instead of the default allocator.

**Goal of this refactor:** Use Linear (frame), Pool, and FreeList allocators **inside eyelib** where hot-path code currently allocates (reserve/push_back, vectors). Do not change architecture; add optional allocator injection and use it for temporaries.

**Where allocators live and who uses them:**

| Allocator | Library (eyelib) | API (api/core) | Used today |
|-----------|------------------|----------------|------------|
| **Linear** | `eyelib/core/allocator/LinearAllocator` | Wrapped as **FrameAllocator** | Player scene build (RenderScene uses frame allocator). |
| **Pool** | `eyelib/core/allocator/PoolAllocator` | **PoolAllocator** (PIMPL) | Not used anywhere. |
| **FreeList** | `eyelib/core/allocator/FreeListAllocator` | **FreeListAllocator** (PIMPL) | Not used anywhere. |

**Refactor scope and messiness (by area):**

1. **Internal RenderScene (eyelib) + API ToInternal** — **Medium–High**
   - **Current:** `eyelib::render::RenderScene` has `std::vector<Drawable> opaque`, etc. API `ToInternal()` in `api/render/RenderPipeline.cpp` builds this with reserve + push_back (heap each frame).
   - **Change:** Make internal RenderScene allocator-aware: optional `IAllocator*` (or linear allocator), STL allocator adapter in eyelib wrapping it, use for the four vectors (opaque, transparent, pointLights, spotLights). API needs a **bridge**: a type that implements eyelib's `IAllocator` and forwards to `api::FrameAllocator`, so `ToInternal(scene, frameAlloc)` can build the internal scene from the frame arena. `Render()` / pipeline entry accepts optional allocator and passes it when building internal scene.
   - **Touch:** `eyelib/render/RenderTypes.h` (allocator adapter, optional allocator in RenderScene), new eyelib allocator adapter for STL (e.g. `core/allocator/LinearAllocatorAdapter.h` or adapter over `IAllocator`), `api/render/RenderPipeline.cpp` (bridge, `ToInternal` and `Render` take optional allocator).
   - **Messiness:** One coordinated change across eyelib + API; all existing call sites that take `const RenderScene&` or `const std::vector<Drawable>&` remain valid.

2. **FrameGraph (eyelib)** — **Medium**
   - **Current:** `m_textures`, `m_passes`, `m_passInfos` are `std::vector<>`; built in AddPass, CreateTexture, Compile.
   - **Change:** FrameGraph accepts an optional allocator (e.g. at construction or at `Clear()`); those three vectors use an STL adapter over that allocator (linear/frame).
   - **Touch:** `eyelib/render/framegraph/FrameGraph.h`, `FrameGraph.cpp` (constructor, Clear, member types).
   - **Messiness:** Single class; localized.

3. **CullingSystem, TransparencyPass, TiledLightGrid, ClusteredLightGrid** — **Low–Medium**
   - **Current:** Fill caller-provided vectors or return `std::vector<>` (heap in API or inside lib).
   - **Change (choose one or both):** (A) Caller uses a vector backed by frame allocator (no library change). (B) Add optional allocator (or scratch-buffer) parameter; library uses it for internal/result buffers.
   - **Touch:** `eyelib/render/CullingSystem.cpp`, `TransparencyPass.cpp`, `TiledLightGrid.cpp`, `ClusteredLightGrid.cpp` — optional allocator param or keep API and document caller-side frame allocator use.
   - **Messiness:** Low if only (A); low–medium per function if (B).

4. **Other renderers (InstancedRenderer, BatchRenderer, DebugDrawRenderer, SpriteRenderer, Primitives, etc.)** — **Low per class, medium in total**
   - **Current:** Member vectors for batches, vertices, indices; grow per frame or over time.
   - **Change:** Optional "scratch" allocator (e.g. per frame from pipeline); use it for those temporary vectors.
   - **Touch:** Each renderer class; same pattern everywhere.
   - **Messiness:** Repetitive but simple.

5. **Resource loaders (Texture, Mesh, Shader, Font, etc.)** — **Low priority**
   - **Current:** `std::vector<uint8_t>` etc. at **load time**, not per-frame.
   - **Change:** Optional scratch allocator for load-time buffers (reduces load-time heap, not frame hitches).
   - **Messiness:** Low per file; can be done later or skipped for first pass.

**Implementation order (recommended):**

1. **Internal RenderScene + API bridge** — Unblocks zero-heap scene path from API through pipeline.
2. **FrameGraph** — One clear boundary (clear each frame); good second step.
3. **Culling / passes / light grids** — Optional allocator or caller-owned buffer; small API additions.
4. **Other renderers** — Optional scratch allocator; repeat same pattern.
5. **Load-time resources** — Optional; later.

**Risks & mitigations:**

- **Risk:** Allocator threading (e.g. frame allocator used from main and render). **Mitigation:** Use one frame allocator per "frame owner"; reset at single well-defined point (e.g. start of frame or start of render); no sharing across threads without sync.
- **Risk:** Internal RenderScene type or ABI change. **Mitigation:** Keep internal scene as "scene with vectors"; only the allocator of those vectors changes; no API/ABI break for `const RenderScene&` / `const std::vector<Drawable>&` consumers.

**Code touch points (summary):**

- **eyelib:** `core/allocator/` (STL allocator adapter over IAllocator or LinearAllocator), `render/RenderTypes.h` (RenderScene allocator-aware), `render/framegraph/FrameGraph.*`, `render/CullingSystem.cpp`, `render/passes/TransparencyPass.cpp`, `render/TiledLightGrid.cpp`, `render/ClusteredLightGrid.cpp`, plus other renderers as needed.
- **api:** `render/RenderPipeline.cpp` (ToInternal + Render take optional allocator; bridge from FrameAllocator to IAllocator).

**Outcome:** Library hot path no longer relies on default allocator for scene build, frame graph, culling, passes, or light grids; Pool/FreeList available for future same-size or variable-size pools inside the library. Phase 4 then fully aligned with Unreal-level memory discipline (client + library).

---

### Phase 5: SIMD (Optional)

**Goal:** Use **SIMD** (SSE/AVX) for bulk math in hot loops: e.g. transform multiply, bounds calculation, culling. Only where profiling shows benefit.

**Work:**

1. **Identify hot loops**
   - Transform application (world matrix build), frustum culling, skinning (vertex transform).
   - Profile first; only vectorize where it matters.

2. **Implementation**
   - Use intrinsics or a small SIMD math layer (e.g. glm SIMD, or custom wrappers).
   - Process 4 or 8 elements at a time; data must be contiguous (Phase 2 SoA helps).

**Code touch points:**

- Math/transform hot paths (e.g. world matrix build, culling).
- Optional: `eyelib/math/` SIMD helpers if we add a small abstraction.

**Risks & mitigations:**

- **Risk:** Platform/ABI differences (SSE vs. AVX, ARM NEON). **Mitigation:** Abstract behind a small layer; runtime dispatch or compile-time options.

**Outcome:** Faster bulk math where applicable. **Unreal/Unity do this for transforms and culling.**

**Priority:** Lower; do after Phase 2 (layout) and when profiling justifies it.

---

### Phase 6: Rendering & Culling (Scale)

**Goal:** **Culling** (frustum, occlusion if needed), **batching**, and **LOD** so that rendering scales to many objects and draw calls are minimized.

**Work:**

1. **Culling**
   - Frustum culling: use world bounds (AABB) per entity or per chunk; run after world read, before building render scene.
   - Can be a job (Phase 3): input = view of transforms + bounds; output = visibility list.

2. **Batching / instancing**
   - Batch identical meshes/materials into instanced draw calls where the pipeline supports it.
   - Reduces draw calls and state changes.

3. **LOD**
   - Distance-based LOD selection (we have LOD data; we need automatic selection by distance).
   - Per-entity or per-chunk LOD; feed into render scene build.

**Code touch points:**

- `eyelib/render/` — culling pass or integration in render scene build; `app/player/WorldRender.cpp`.
- Render pipeline (deferred/forward/forward+) — instancing and LOD selection.

**Risks & mitigations:**

- **Risk:** Culling too expensive. **Mitigation:** Run as job; use coarse bounds first; hierarchical where needed.

**Outcome:** Rendering scales to large scenes; fewer draw calls; **Unreal-level scalability** on the GPU side.

**Priority:** Medium; can overlap with Phase 3 (culling as a job).

---

## 6. Priority & Dependency Summary

| Phase | Content | Priority | Depends On |
|-------|---------|----------|------------|
| **1** | Zero-copy read path (views / spans / fill-buffer) | **High** | — |
| **2** | Cache-friendly layout (SoA / chunks) | **High** | Phase 1 |
| **3** | Job system & parallelism | **Medium–High** | Phase 2 |
| **3b** | Render thread & command list (Unreal-level multithreading) | **Medium** | Phase 3 |
| **4** | Memory (frame allocator, pools, reuse) | **Medium** | Phase 1 |
| **5** | SIMD | **Lower** | Phase 2 |
| **6** | Culling, batching, LOD | **Medium** | Phase 2, 3 |

**Recommended order:** 1 → 2 → 4 (basic) and 3 (parallelism) in parallel with 6 (culling). Phase 3b (render thread) when you need game/render overlap. Phase 5 when profiling shows need.

---

## 7. Metrics & How to Measure

- **Phase 1:** Allocation count per frame (e.g. no new vectors in Observation getters); optional: bytes copied per frame (target: zero for batch getters).
- **Phase 2:** Cache-friendly iteration — measure cache miss rate or cycle count for “iterate all transforms”; iteration should be contiguous.
- **Phase 3:** CPU utilization (multiple cores active during frame); frame time reduction for CPU-bound scenes.
- **Phase 4:** Allocations per frame (target: minimal or zero in hot path); frame time variance (smoother = less allocator pressure).
- **Phase 5:** Cycle count or time for bulk transform/culling loops before vs. after SIMD.
- **Phase 6:** Draw call count, triangles submitted vs. visible; frame time vs. object count (should scale sub-linearly with culling/batching).

**Tools:** Profiler (e.g. Tracy, Visual Studio profiler); allocation tracking; simple in-game timers for phase boundaries.

---

## 8. Success Criteria

- **Phase 1:** No per-frame allocation/copy of full transform/renderable/light arrays; hot path uses views or single reused buffer.
- **Phase 2:** At least Transform (and optionally other hot components) in contiguous, SoA-like storage; iteration is cache-friendly.
- **Phase 3:** Multiple cores used during frame; physics/animation/culling can run in parallel with clear sync points.
- **Phase 4:** Frame allocator in use for hot-path temporaries; no unnecessary malloc/free per frame.
- **Phase 5:** Key bulk math loops (e.g. transform, culling) vectorized where measurable gain.
- **Phase 6:** Frustum culling active; batching/LOD where applicable; render scene build scales with object count.

**Overall:** Runtime hot path **pattern** and **efficiency level** match professional engines (Unreal/Unity): zero-copy reads, cache-friendly layout, parallelism, low allocation, and scalable rendering.

---

## 9. What We Are Not Changing

- **World model:** WorldState remains the single source of truth; we are not replacing it with a different ECS.
- **Mutation model:** Commands and validation stay; we are not moving to lock-free or concurrent mutation.
- **Serialization:** WorldSerializer + WorldState remain canonical; this plan does not change save/load format.
- **API contract:** Observation remains the read path; we are adding view/buffer APIs, not removing the abstraction.

---

## 10. References

- **Partials_Fix_Plan.md** — Observation layer, mutation, serialization (done).
- **Neutral_In_Memory_Representation.md** — Load path and format-agnostic design (optional later).
- **Runtime_Player_And_Shipping.md** — What the runtime does and how it ships.
- **Serialization_Authority.md** — One serializer, WorldState only.

---

## 11. Document History

- **Created:** Plan for Unreal-level runtime efficiency.
- **Phases:** 1 (views) → 2 (layout) → 3 (jobs) → 4 (memory) → 5 (SIMD) → 6 (culling/batching/LOD).
- **Expanded:** Comparison table, code touch points, metrics, risks & mitigations, success criteria.
- **Phase 1 done:** Added `GetXxxInto(out)` and `GetXxxView(out)` to Observation; WorldRender, WorldPhysics, WorldAudio use reused buffers; Lua batch getters use thread_local buffers + GetXxxInto. Removed deprecated vector-returning getters. No per-frame allocation in hot path for batch reads.
- **Phase 2 done (Transform):** SoA store for Transform: `ComponentStoreTransform.h` specialization with `position[]`, `rotation[]`, `scale[]`, `entityToIndex`; `All()` returns lazy `ConstRange` (no alloc); `PositionsSpan()`, `RotationsSpan()`, `ScalesSpan()`, `EntitiesSpan()` for cache-friendly iteration. WorldState includes specialization; serialization and commands unchanged. Observation::GetTransformsInto uses SoA spans for tight fill loop.
- **Phase 2 done (all hot components):** Dense stores for Camera, Light, MeshRenderer, Renderable, RigidBody, Collider in `ComponentStoreDense.h` (dense `vector<T>` + `indexToEntity` + `entityToIndex`; `All()` lazy range; `EntitiesSpan()`, `ComponentsSpan()`). WorldState includes `ComponentStoreDense.h`. All Observation::GetXxxInto use spans for tight fill loops. Phase 2 complete.
- **Phase 3 done:** Job-based frame: `WorldPhysicsBridge::StepPhysicsOnly(deltaSeconds, maxSubSteps, fixedTimeStep)` (runs on worker; no world access); `SyncTransformsToWorld(World&)` (main thread after Wait). PlayerApp uses **API only**: `eyelib::api::core::ThreadPool` (size 0 = hardware_concurrency), no direct `eyelib` includes in player. Each frame: SubmitBatch({ StepPhysicsOnly, AnimationBridge::Update }), Wait(), then SyncTransformsToWorld(world), audio, scripting, world.Update(deltaSeconds). Physics and animation run in parallel; no world mutation in jobs; sync on main. Existing `TaskGraph` in api/core/Job.h available for dependency-based jobs. Phase 3 complete.
- **Phase 4 done:** Memory strategy: **Frame allocator** in `api/core/FrameAllocator.h` (PIMPL over `eyelib::core::allocator::LinearAllocator`): bump-pointer block (default 256 KB), `Allocate(size, alignment)`, `Reset()` each frame; on block overflow falls back to heap with warning, freed on next Reset(). Exposed as `eyelib::api::core::FrameAllocator` via All.h. **PoolAllocator** and **FreeListAllocator** in `api/core/` (PIMPL over eyelib implementations); exposed via All.h. **FrameAllocatorAdaptor** in `api/core/FrameAllocatorAdaptor.h` (STL allocator wrapping FrameAllocator*) for use with `std::vector<T, FrameAllocatorAdaptor<T>>`. **RenderScene** (`api/render/RenderScene.h`) uses FrameAllocatorAdaptor for opaque, transparent, pointLights, spotLights; constructor `RenderScene(::api::FrameAllocator* frameAlloc)` builds scene with arena-backed vectors. **Reuse buffers:** PlayerApp resets frame allocator at start of each update; jobs list is a reused `frameJobs` vector. **Player scene build:** WorldRenderBridge owns `m_sceneFrameAllocator`; each Render() resets it and builds `RenderScene(&m_sceneFrameAllocator)` so scene vectors use the arena (no heap in that hot path). Observation and hot-path callers already use caller-owned buffers (Phase 1). Phase 4 (client/API side) complete.
- **Phase 4 refactor (planned):** Comprehensive plan added in Phase 4: **Library-side allocator use**. eyelib implements Linear, Pool, FreeList but no library module uses them yet. Refactor plan documents: (1) Internal RenderScene (eyelib) + API ToInternal bridge — allocator-aware internal scene, API bridge from FrameAllocator to IAllocator; (2) FrameGraph optional allocator; (3) CullingSystem, TransparencyPass, TiledLightGrid, ClusteredLightGrid — optional allocator or caller-owned buffer; (4) Other renderers — optional scratch allocator; (5) Load-time resources — optional, later. Implementation order: Internal RenderScene + bridge → FrameGraph → culling/passes/grids → renderers → load-time. Code touch points and risks documented. Implementation to follow.
- **Phase 3b added:** Render thread & command list (Unreal-level multithreading): what to change (double-buffer scene, render thread loop, no GL on game thread, optional command list), benefits (overlap, pacing, scalability), level of change (high in app/player, low in pipeline), recommended order and risks. Priority: medium, after Phase 3.
- **Next step:** Implement Phase 4 refactor (library-side allocator use); then Phase 5 (SIMD) or Phase 6 (culling, batching, LOD) when ready. Phase 3b when game/render overlap is needed.
- **Phase 1 (view/span) done:** WorldObservation and api::Observation expose GetXxxView() returning std::span over reused mutable buffers. No vector-returning batch getters; hot path (WorldRender, WorldPhysics, WorldAudio, Lua) uses GetXxxView() only. View valid until next GetXxxView() or world mutation. Zero per-call allocation; buffers reused each frame.
