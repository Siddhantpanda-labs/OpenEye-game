# Render Architecture Refactor — Clean, Production-Level Plan

This document is the **single source of truth** for refactoring the OpenEye render module into a clean, maintainable, production-grade architecture (Unreal-style separation of concerns, clear ownership, scalable submission). Follow phases in order; each phase is implementable without breaking the build.

**Related docs:**
- **Efficiency (FPS, compile-once, scene build, instancing):** `documentation/refractor/Pipeline_efficiency_roadmap.md`
- **Current module layout:** `EngineSource/eyelib/render/RENDER_MODULE.md`

---

## 1. Goals

- **Single responsibility** per subsystem: no 1800-line orchestrator owning init, frame graph, shadows, forward, deferred, stereo, post, debug, and streaming.
- **Clear data flow:** immutable per-frame input (`RenderFrameData`), cached static/world data (`RenderWorldCache`), explicit submission (batches, not ad-hoc per-draw).
- **Unified pipeline:** all modes (Forward, Deferred, Stereo) go through the same frame-graph and submission abstractions; no legacy fork.
- **Testable and profileable:** subsystems can be tested in isolation; profiler hooks and metrics are first-class.
- **Editor vs runtime:** editor-only features (picking, outline, debug overlays) live in an editor layer that calls into the core renderer; core stays runtime-agnostic.

---

## 2. Current State (Summary)

- **Strengths:** RHI abstraction, `IRenderSceneView` zero-copy, frame graph DAG, CPU/GPU mesh split, explicit device passing.
- **Debt:** `RenderPipeline` is a monolith (~1793 lines); forward/deferred/stereo/forward+ duplicate logic; submission is per-draw with high bind/uniform churn; no clear frame-data vs world-cache split.

---

## 3. Target Architecture (Principles)

| Principle | Meaning |
|-----------|--------|
| **Coordinator per concern** | Frame graph build/execute, shadows, geometry submission, lighting, post-FX are separate systems; pipeline only wires them. |
| **Frame data vs world cache** | Per-frame: camera, view, list of visible drawables (result of culling), light list. Cached: mesh/material/shader lookups, static scene structures. |
| **Submission layer** | One place that turns “visible drawables + pass” into GPU commands: sort keys, material/pass buckets, instancing; no per-draw logic in pipeline. |
| **Mode-agnostic graph** | Forward and Deferred differ only by recipe (pass list) and which passes run; same compile/execute and same submission interface. |
| **Editor on top** | Entity ID pass, outline, debug lines, profiler overlay are optional features that feed into or wrap the core render path. |

---

## 4. Refactor Phases (Order of Execution)

### Phase A: Extract Frame-Graph Coordination

**Goal:** Frame graph build, compile, and execute are owned by a dedicated component; `RenderPipeline` only holds it and invokes it with current frame data.

**Tasks:**

1. **Introduce `FrameGraphCoordinator`** (or keep logic in pipeline but behind a clear API):
   - `SetRecipe(recipe)` / `SetFrameSize(w,h)` / `CompileIfNeeded(device)` / `Execute(ctx)`.
   - Recipe and nameToHandle built from pipeline-owned resources (HDR, GBuffer, Shadow, etc.); executors still receive “current view” from pipeline (e.g. `GetCurrentView()`).
   - Move all `BuildAndExecute*` frame-graph construction into this coordinator (or into a single `BuildFrameGraph(...)` function that returns a compiled graph state).

2. **Pipeline’s only frame-graph duties:** Provide resource handles (HDR, GBuffer, Shadow RTs), provide per-frame view/camera/light, call coordinator’s `CompileIfNeeded` and `Execute`.

3. **Success criteria:** No behavior change. Frame graph compile/execute logic is in one place; pipeline code that builds the graph is readable and under 150 lines per mode.

**Files:** `eyelib/render/renderer/RenderPipeline.cpp` (.h), optionally `eyelib/render/framegraph/FrameGraphCoordinator.h` (.cpp) if you extract a class.

---

### Phase B: RenderFrameData and RenderWorldCache

**Goal:** Explicit immutable snapshot for the frame (camera, view, lights, visible drawables); cached world-side data (resolved meshes, materials, static buffers) so submission and culling don’t re-resolve every frame.

**Tasks:**

1. **Define `RenderFrameData`** (e.g. in `RenderTypes.h` or `RenderFrameData.h`):
   - Camera view, projection, view pos, frustum.
   - Directional light (and optionally point/spot arrays).
   - List of visible opaque drawables (result of culling; references to mesh/material + transform only).
   - Optional: visible transparent list (sorted).
   - Filled once per frame by a “prepare” step (after scene build and culling).

2. **Define `RenderWorldCache` (or equivalent) for static/cached data:**
   - Resolved mesh/material per entity or per drawable key (you already have partial cache in `EditorSceneBuilder`; generalize or mirror in render layer).
   - Optional: instance buffers, indirect buffers, or other GPU state that persists across frames.
   - Invalidated when scene/world changes (e.g. scene revision, load/unload).

3. **Pipeline (or a “PrepareFrame” step) fills `RenderFrameData` from `IRenderSceneView` + culling; passes only `RenderFrameData` (and device/cache) into submission and passes.**

4. **Success criteria:** One clear place that builds “what to draw this frame”; submission and passes take `const RenderFrameData&` (or equivalent); no per-draw resolution inside the hot path.

**Files:** `eyelib/render/RenderTypes.h`, new `eyelib/render/RenderFrameData.h` (optional), `eyelib/render/renderer/RenderPipeline.cpp`, and any place that currently iterates `ForEachOpaqueDrawable` and does work (passes, GBuffer, forward).

---

### Phase C: Unify All Modes Through Frame Graph

**Goal:** Forward, Deferred, and Stereo are three recipes on the same frame graph and same submission path; remove or isolate the legacy Forward+ branch.

**Tasks:**

1. **Represent Forward, Deferred, Stereo as three recipes:**
   - Same `FrameGraph` API: same `Compile(device)`, `Execute(ctx)`.
   - Only the list of passes and their read/write resources differ (and stereo uses a different vertex path with two viewports).

2. **Forward+ (tiled/clustered):** Either migrate to a frame-graph pass that runs a “forward tiled” pass, or keep it as a separate code path but behind a single entry point (e.g. `RenderForwardPlus(view)` that builds a minimal graph or a dedicated path). No duplicated init/shadow/light logic.

3. **Success criteria:** One code path that “builds recipe for mode M and executes”; no copy-paste of shadow setup, lighting buffer, or pass order across four places.

**Files:** `eyelib/render/renderer/RenderPipeline.cpp`, frame-graph recipe construction.

---

### Phase D: Submission Layer (Batching, Sort Key, Instancing)

**Goal:** A single submission layer that takes `RenderFrameData` + pass type (e.g. GBuffer, Shadow, Forward Opaque) and emits GPU commands in batches (by material/pass/sort key); supports instancing where mesh+material match.

**Tasks:**

1. **Define a “draw batch” or “command bucket” abstraction:**
   - Input: list of drawables (or visible set), pass (e.g. depth, gbuffer, forward).
   - Output: ordered batches (e.g. by shader, then material, then mesh); each batch = one bind + N draws (or one instanced draw with N instances).

2. **Integrate with existing passes:** GBufferPass, ShadowPass, and forward opaque submission call into this layer (or use a shared “submit visible set for pass P” API) instead of ad-hoc per-draw loops.

3. **Instancing:** When building batches, group by (mesh, material); for each group with >1 drawable, fill instance buffer and issue `DrawIndexedInstanced`; otherwise single draw. Reuse or add instanced shader variants.

4. **Success criteria:** Draw call count scales with unique (mesh, material) per pass; no per-draw material bind in hot path; submission logic is in one place and testable.

**Files:** New `eyelib/render/submission/` (e.g. `SubmissionLayer.h/.cpp`, `DrawBatch.h`), then `GBufferPass.cpp`, `ShadowPass.cpp`, `RenderPipeline.cpp` (forward opaque) call into it. Align with **Pipeline_efficiency_roadmap Phase 4** (GPU instancing).

---

### Phase E: Threading (Render Prep on Workers)

**Goal:** Visibility/culling, sort-key computation, and batch building run on worker threads; only final submission (command list record) runs on main thread.

**Tasks:**

1. **Identify thread-safe inputs:** `IRenderSceneView` or a snapshot of it (or `RenderFrameData` built from it) must be readable from a worker (e.g. copy minimal data at start of frame).

2. **Run on workers:** Frustum culling, optional occlusion, sort key per drawable, and “build batch list” (no GPU calls). Result: a list of batches (or indirect args) that the main thread will consume.

3. **Main thread:** Takes batch list, records commands (bind pipeline, bind resources, draw). No heavy CPU work in this path.

4. **Success criteria:** Profiler shows culling/batch build off main thread; main-thread render time drops; no GPU calls from worker threads.

**Files:** New job system or use existing; `RenderPipeline` or a “RenderScheduler” dispatches prep jobs and waits before submission. Optional after Phases A–D if you prefer to optimize single-thread first.

---

### Phase F: Editor / Runtime Split

**Goal:** Entity ID pass, outline pass, debug lines, and profiler overlay are clearly editor-only; core renderer has no knowledge of “selection” or “editor camera”; API layer or editor app wires these on top.

**Tasks:**

1. **Entity ID and outline:** Triggered only when editor provides `entityIdsForOpaque` and a valid entity ID RT; pipeline exposes “run entity ID pass” and “run outline pass” as optional steps after main render. No branching in core path when not used.

2. **Debug lines / debug draw:** Injected via a callback or a “debug command list” that the pipeline executes before/after post; not mixed with main scene submission.

3. **Profiler:** Already in editor; ensure it only samples and displays; no profiler logic inside core render hot path.

4. **Success criteria:** Standalone runtime (no editor) builds and runs with no editor-only code in the core render path; editor adds features by composition (call pipeline, then call outline, then present).

**Files:** `eyelib/render/renderer/RenderPipeline.cpp` (optional steps guarded by “if (entityIdsForOpaque)” etc.), `api/render`, `app/editor` (outline, picking, profiler UI).

---

## 5. Relationship to Pipeline_efficiency_roadmap.md

| Roadmap phase | Render_refractor alignment |
|---------------|----------------------------|
| 1 — Frame graph compile once | Already done; Phase A keeps this and clarifies ownership. |
| 2 — Incremental scene build | In progress (EditorSceneBuilder); Phase B’s cache aligns with “dirty scene” and stable drawable list. |
| 3 — Async mesh + deferred upload | Independent; do in parallel or after Phase B. |
| 4 — GPU instancing | Implemented inside **Phase D** (submission layer). |
| 5 — Zero-copy mesh (optional) | Independent; can follow Phase D. |
| 6 — Consolidation and polish | Matches **Phase F** and final audit; also “single source of truth” from Phase A–D. |

Implement efficiency roadmap phases 1–2 (and 3 if you want) as now; use **Render_refractor** Phases A–D to restructure the pipeline and submission so that roadmap 4–6 land in the right place.

---

## 6. Implementation Order (Recommended)

1. **Phase A** — Extract frame-graph coordination (no behavior change).
2. **Phase B** — Introduce RenderFrameData (and optional RenderWorldCache); pipeline and passes consume it.
3. **Phase C** — Unify Forward/Deferred/Stereo recipes; remove duplication.
4. **Phase D** — Submission layer + batching + instancing (fulfills roadmap Phase 4).
5. **Phase E** — Threading (optional; after D if needed).
6. **Phase F** — Editor/runtime split and polish.

Each phase ends with a green build and no regressions; then update the Implemented table below.

---

## 7. Implemented

Update this section as each phase is completed. Use **Done** only when the implementation is production-quality and meets the phase success criteria.

| Phase | Status | Notes |
|-------|--------|--------|
| A — Frame-graph coordination | Done | CompileFrameGraphIfNeeded(mode, stereo) + BuildDeferredGraph/BuildForwardGraph/BuildForwardStereoGraph; BuildAndExecute* now set state, prepare, CompileFrameGraphIfNeeded, Execute. No behavior change. |
| B — RenderFrameData / RenderWorldCache | Done | RenderFrameData in RenderTypes.h (camera, light, skybox, point/spot, visibleOpaque, allOpaque, visibleTransparent). PrepareFrameData(view) + PrepareGpuCullFromFrameData(). Executors use m_currentFrameData; GBuffer/forward/shadow/transparency/skybox take frame data. RenderWorldCache left to EditorSceneBuilder. |
| C — Unify modes through frame graph | Done | Deferred/Forward/Stereo already use same frame-graph flow (Phase A). Forward+ now uses PrepareFrameData + m_frameData (shadow feed, lights, draw loop, transparency); no duplicate scene/light gather or shadow drawable source. Single entry point: Render(view) switch. |
| D — Submission layer (batching, instancing) | Done | DrawBatch + BuildBatches in submission/; GBuffer and Shadow use batches (instanced draw for multi-instance, singles feed otherwise). Pipeline owns m_instanceMatrixBuffer (4K mat4s); GBufferPass/ShadowPass RenderInstanced take submeshIndex. Forward+ opaque now batched (forwardplus_instanced shader + binding 6); frame-graph Forward+ uses BuildForwardPlusGraph, PrepareForwardPlusFrame, RenderForwardPlusOpaqueBatched. Forward (non-Plus) opaque still per-draw. |
| E — Threading (render prep on workers) | Not started | |
| F — Editor/runtime split | Not started | |

---

## 8. Conventions (After Refactor)

- **Device:** Always pass `rhi::IDevice*` (or `api::RenderDevice`); no global device.
- **Scene input:** Pipeline receives `IRenderSceneView` or a prepared `RenderFrameData`; no copy of full scene inside pipeline.
- **Frame graph:** Single coordinator builds/compiles/executes; recipe is mode-dependent; executors receive current frame data from pipeline or context.
- **Submission:** One layer turns “visible set + pass” into batches and GPU commands; passes call into it.
- **Editor:** Picking, outline, debug draw, profiler are optional add-ons on top of core render API.

---

*Document: Render architecture refactor — clean, production-level plan.*  
*Location: `documentation/refractor/Render_refractor.md`.*
