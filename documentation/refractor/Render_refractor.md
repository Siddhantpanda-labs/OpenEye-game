# Render Module Refactor Roadmap (Operational)

This document is the **working companion** to `documentation/refractor/Render_Report.md`. The report is the **audit and target blueprint**; this file is the **roadmap**: how to move toward an industry-grade, clean, scalable renderer **without breaking features** and **without big-bang rewrites**.

---

## 1) Purpose

- Align day-to-day work with a **single architecture contract**.
- Define what “10/10” means in **measurable** terms (not vanity).
- Record **current status** after incremental refactors.
- List **ordered, safe steps** that teams can execute in small PRs.

Non-goals:

- Rewriting the RHI or backends from scratch.
- Renaming every file in one pass.
- Dropping editor, stereo, or optional backends for “cleanliness.”

---

## 2) What “10/10” Means (Professional, Clean, Scalable)

These are **independent** dimensions. A codebase can be strong in one and weak in another; the goal is to raise all three.

### 2.1 Professional (engineering quality)


| Criterion              | At target (10)                                                                                                                     |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| **Ownership**          | Every domain has one obvious owner (`*PassSystem` or a named service). No “half in pipeline, half in a helper.”                    |
| **Contracts**          | Passes expose predictable **BuildParams → Execute** (or equivalent) boundaries; no hidden globals.                                 |
| **Backend discipline** | Capability and branching go through `**RenderBackendPolicy`** (and related helpers), not scattered `if (Vulkan)` in orchestration. |
| **Reviewability**      | `RenderPipeline` changes are mostly wiring; pass logic changes live in pass systems.                                               |


### 2.2 Clean (structure and clarity)


| Criterion             | At target (10)                                                                                                         |
| --------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| **Thin orchestrator** | `RenderPipeline` schedules mode, frame context, and graph execution; it does not own large draw loops.                 |
| **Naming**            | `FrameGraph` vs `RenderGraph*` naming is documented; `*Pass` vs `*PassSystem` roles are explicit.                      |
| **File boundaries**   | Shared contexts live in small headers (e.g. `ForwardGeometryCommon.h`); no duplicate declarations across pass headers. |
| **Documentation**     | `render_struc.md`/`Render_Report.md` stay **true to the tree** (updated when ownership moves).                         |


### 2.3 Scalable (team and feature growth)


| Criterion             | At target (10)                                                                           |
| --------------------- | ---------------------------------------------------------------------------------------- |
| **Graph-first**       | New modes add or reorder **recipes**, not fork entire pipelines.                         |
| **Hot-file pressure** | Fewer engineers need to edit `RenderPipeline.cpp` for typical features.                  |
| **ABI hygiene**       | Push constants / shared GPU ABI are **domain-scoped** (split from a single mega-header). |
| **Regression safety** | Refactors are **incremental** with build + targeted runtime checks.                      |


---

## 3) Current Status (Snapshot)

This section should be **updated when major ownership moves land**. As of the last refactor pass documented here:

### 3.1 Strengths already in place

- **Multi-backend RHI** (OpenGL, Vulkan, optional Diligent) and `**RenderBackendPolicy`**.
- **Frame graph** (`framegraph/`) and **recipe-driven** execution (`renderer/graph/`).
- **Pass systems** own or encapsulate major paths: e.g. **GBuffer** deferred geometry (`GBufferPassSystem`), **forward** opaque/transparent/stereo/forward+** extracted to dedicated geometry/prep sources with `**ForwardGeometryCommon.h`**, **Skybox** resources and draw orchestration in `**SkyboxPassSystem`**, plus existing systems for shadow, deferred lighting, transparency, post, overlays, streaming, etc.
- **Shared resources** pattern (`PipelineSharedResources`) for shader reuse across pipelines.

### 3.2 Remaining gaps (honest)

- `**RenderPipeline.cpp` is still large** (currently smaller than before after graph extraction, but still a hot integration file).
- **Hybrid pass model** remains: classical `*Pass` types (**Shadow**, **Lighting**, **Transparency**, **PostProcessor**, etc.) still exist alongside `*PassSystem` wrappers; **not all** are fully collapsed into “system owns implementation.”
- **Cross-domain ABI** (`PassPushConstants.h`) can still grow; **splitting by domain** is a deliberate future step.
- **Docs drift**: `render_struc.md` / `Render_Report.md` must be **kept in sync** when files and ownership move.

### 3.3 Recently completed milestones

- **Phase A landed**:
  - `GpuCullScenePrep` folded into `GpuCullPassSystem` (types + helpers consolidated).
  - `GpuCullPass` moved under `passes/detail/` and graph execution routed through `GpuCullPassSystem::ExecuteGpuCull`.
  - `LightCullPass` moved from `lighting/` to `passes/detail/` alongside `LightCullPassSystem`.
- **Phase B landed**:
  - Graph executor routing consolidated into a single resolver path (`ExecutorFor(set, name)`).
  - Shared pass-name constants introduced (`renderer/graph/FrameGraphPassNames.h`) and used by recipes.
  - Graph/executor wiring extracted into `RenderPipelineGraph.cpp`; `RenderPipeline.cpp` now focuses more on orchestration/frame work.

---

## 4) Principles (Non-Negotiable for Safe Refactors)

1. **Incremental extraction**
  Move one vertical slice at a time (e.g. “skybox,” “GPU cull prep,” “shadow batched path”). After each slice: **full build**, smoke test, then merge.
2. **Behavior parity**
  Prefer **copy-move-delete**: extract implementation into the system, wire pipeline to call it, **delete** old paths only when nothing references them.
3. **Single owner per domain**
  After a move, the pipeline should **not** retain duplicate shader loads, descriptor rings, or draw loops for the same feature.
4. **Preserve public engine boundaries**
  `api/render` wrappers stay thin; refactors stay in `**eyelib/render`** unless the API contract is intentionally updated.
5. **Do not delete “important but quiet” modules**
  e.g. `InstancedRenderer.cpp` / `BatchRenderer.cpp` remain until a true duplicate is proven, not “unused.”
6. **Tests and manual checks**
  At minimum: **Debug build**, **editor or viewer** run, **deferred + forward + forward+** paths if touched, **stereo** if graph executors change.

---

## 5) Roadmap: Ordered Steps Toward 10/10

Execute in order when possible; each step is a **separate PR** or small series.

### Phase A — Consolidate ownership (high impact, low risk)


| Step   | Action                                                                                                                                          | Success criteria                                                                                                                                              |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **A1** | Fold `**GpuCullScenePrep`** into `**GpuCullPassSystem**` (types + helpers in one module; optional rename of params structs).                    | ✅ Completed — no standalone `GpuCullScenePrep` module; pipeline still builds `GpuCullScenePrepUploadParams`; build + cull behavior preserved. |
| **A2** | Optionally make `**GpuCullPass`** a **private implementation detail** of `GpuCullPassSystem` (same as GBuffer pattern: one public system type). | ✅ Completed (non-strict variant) — `GpuCullPass` moved to `passes/detail/`, graph execute path goes through `GpuCullPassSystem`; strict “no external include anywhere” remains optional hardening. |
| **A3** | Apply the same **“system owns resources + impl”** pattern to **light cull** if `LightCullPass` + `LightCullPassSystem` remain split.            | ✅ Completed — `LightCullPass` moved to `passes/detail/` with `LightCullPassSystem` as owner. |


### Phase B — Thin `RenderPipeline` (orchestration only)


| Step   | Action                                                                                                                                                                                    | Success criteria                                                   |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| **B1** | Move **graph executor registration** toward a **single table** (pass name → lambda) where possible; reduce copy-paste between deferred/forward/stereo.                                    | ✅ Completed — unified executor path (`ExecutorFor(set, name)`) with shared pass names and retained behavior.    |
| **B2** | Extract **“frame context builder”** helpers (already partially via `FrameRenderContext`) so pipeline methods are shorter.                                                                 | ✅ Completed (incremental) — shared render-context helper is used in frame-graph execution paths; orchestration methods are thinner.         |
| **B3** | Split `**RenderPipeline.cpp`** into **2–3 translation units** by **domain** (e.g. `RenderPipelineGraph.cpp`, `RenderPipelineFrame.cpp`) only if includes stay clean—**no circular deps**. | ✅ Partially completed — `RenderPipelineGraph.cpp` extracted and building cleanly; further split remains optional follow-up. |


### Phase C — Uniform pass contract


| Step   | Action                                                                                                                                                                                     | Success criteria                                               |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------- |
| **C1** | For each remaining classical pass still used from the graph, ensure `**XxxPassBuildParams` / `XxxPassExecuteParams`** (or documented equivalent) and `**BuildParams` + `Execute**` naming. | New contributors can follow one pattern.                       |
| **C2** | **Mutation boundary**: document and enforce “descriptor/buffer updates in build/setup, execute binds/draws only” for new code; refactor old code opportunistically.                        | Fewer “update descriptor set in the middle of draw” surprises. |


### Phase D — Backend and ABI hygiene


| Step   | Action                                                                                        | Success criteria                                           |
| ------ | --------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| **D1** | Route new backend checks through `**RenderBackendPolicy`** (and related helpers).             | Orchestration files contain minimal API-specific branches. |             
| **D2** | Split `**PassPushConstants.h`** by domain (forward vs deferred vs post) behind thin includes. | Smaller rebuilds, clearer ownership.                       |


### Phase E — Folder and documentation governance


| Step   | Action                                                                                                                                                                                                  | Success criteria                          |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------- |
| **E1** | Align `**render_struc.md`** with the **actual** tree (remove stale references like deleted passes).                                                                                                     | Docs match reality.                       |
| **E2** | Optional **folder migration** toward `Render_Report.md` Section 7 (`passes/deferred`, `passes/forward`, …) **only** when a subdirectory is large enough to justify; use **git mv** to preserve history. | No functional change; clearer navigation. |


---

## 6) How to Measure Progress

Use these **practical metrics** (update periodically):


| Metric                                                       | Target direction                                           |
| ------------------------------------------------------------ | ---------------------------------------------------------- |
| Lines in `RenderPipeline.cpp`                                | Down over time (or split across TUs without total growth). |
| Count of `*Pass` types directly called from pipeline         | Down; systems own execution.                               |
| Number of duplicate `Build*` / draw paths for same feature   | Zero.                                                      |
| `RenderBackendPolicy` usage vs raw API checks in `renderer/` | Policy usage up.                                           |
| PR size for “add a pass”                                     | Mostly recipe + one system, not pipeline surgery.          |


---

## 7) Relationship to Other Docs


| Document                              | Role                                                                 |
| ------------------------------------- | -------------------------------------------------------------------- |
| `**Render_Report.md`**                | Full audit, blueprint, ultimate folder layout, ownership rules.      |
| `**render_struc.md**`                 | Current vs ultimate map; **must be updated** when structure changes. |
| `**Render_refractor.md` (this file)** | **Operational roadmap**: steps, safety, and “what good looks like.”  |


---

## 8) Final Note

A **10/10** renderer module is not a single refactor. It is **disciplined governance** over time: thin orchestration, **one owner per pass domain**, **graph composition over duplication**, **policy-centered backend decisions**, and **incremental** extractions with **build + runtime** verification. This roadmap is written to be **actionable** and **safe**—follow the phases, keep PRs small, and update this snapshot when major milestones land.