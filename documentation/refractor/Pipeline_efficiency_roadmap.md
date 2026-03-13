# Pipeline Efficiency Roadmap — Sequential Implementation Plan

This document defines a **professional, ordered plan** to make the OpenEye rendering and editor pipeline clean, efficient, and high-FPS (Unity/Unreal level). Implement **each phase in sequence**; later phases assume earlier ones are in place.

**Approach:** Each phase is implemented properly—no workarounds or temporary patches. Solutions are architectural and maintainable. Track completion in the **Implemented** section at the bottom.

**Architecture refactor (clean structure, coordinators, submission layer):** See `documentation/refractor/Render_refractor.md` for the plan to split the pipeline into coordinators, introduce RenderFrameData/RenderWorldCache, and unify modes. That plan and this roadmap are complementary: this one focuses on FPS and per-frame cost; that one on structure and production-level design.

---

## Goals

- **No sustained FPS drop** when a model is in the scene (per-frame cost minimal).
- **No frame hitch** on first load (async load + deferred GPU upload).
- **Scalable** draw-call and CPU cost (instancing, dirty scene, frame graph compile once).
- **Clear ownership** and no redundant work (single source of truth, incremental updates).

---

## Phase 1: Frame Graph — Compile Once, Execute Every Frame

**Problem:** `FrameGraph::Clear()` + `AddPassesFromRecipe()` + `Compile(device)` runs every frame. The pass DAG and resource layout are constant for a given resolution and recipe; only execution and bound resources change.

**Goal:** Build the frame graph once (or when recipe/resolution changes); each frame only call `Execute(ctx)`.

**Tasks (in order):**

1. **Add a “dirty” or “compiled” flag** (e.g. on `RenderPipeline` or `FrameGraph`):  
   - Set when: recipe changes, resolution changes, or first run.  
   - Clear when: after a successful `Compile(device)`.

2. **Change the render path:**  
   - If not compiled (or recipe/resolution changed): call `Clear()`, `SetFrameSize()`, `AddPassesFromRecipe(...)`, then `Compile(device)`. Set compiled = true.  
   - Every frame: call only `Execute(ctx)` (no Clear/AddPasses/Compile).

3. **Ensure pass executors close over current frame data:**  
   - Executors (lambdas) already capture `view`, `cam`, etc. by reference when building the recipe. Ensure the recipe builder is called only when we actually rebuild the graph (e.g. once per compile), and that `Execute` uses the **current** view/camera/context.  
   - If executors are stored in the graph, they must receive current scene/camera data at execute time (e.g. via a context struct passed into `Execute(ctx, frameData)` or by updating a pipeline-owned “current frame” state before `Execute`).

4. **Handle resolution change:**  
   - On `Resize(w, h)`, set compiled = false so next frame (or next render) recompiles with new size.

5. **Validate:** Run with one model in scene; confirm only one Compile (e.g. log or breakpoint), and that FPS stays high.

**Files to touch:**  
`eyelib/render/renderer/RenderPipeline.cpp`, `eyelib/render/framegraph/FrameGraph.cpp` (and `.h` if needed for a “needs compile” API).

**Success criteria:** Frame graph is compiled only when necessary; per-frame cost no longer includes DAG build, lifetime analysis, or aliasing.

---

## Phase 2: Scene Build — Incremental / Dirty Updates

**Problem:** Every frame, `EditorSceneBuilder::Build()` clears `opaqueStorage` (and related) and rebuilds the full drawable list from `RenderSnapshot`. Work is O(entities) every frame even when nothing changed.

**Goal:** Rebuild the drawable list only when the snapshot or asset references actually change (dirty scene); otherwise reuse the previous frame’s list.

**Tasks (in order):**

1. **Define “scene dirty”:**  
   - Track a generation or hash of: entity set, component set (renderable/meshRenderer), mesh/material asset IDs, transform version (e.g. from `FlushTransforms` or world revision).  
   - Option A: store last snapshot hash/generation; if equal to current, skip full rebuild.  
   - Option B: maintain a “scene revision” on World (incremented when entities/components/transforms change); compare with last built revision.

2. **Use a stable drawable list:**  
   - Keep the previous frame’s `opaqueStorage` (and drawable-to-entity mapping) as the “current” list.  
   - Only when dirty: clear, iterate snapshot, resolve assets, build new list, swap or replace.

3. **Update transforms in place (optional but recommended):**  
   - If only transforms changed (same entities, same meshes), avoid re-resolving assets; just update `drawable.transform` (and possibly bounds) from the snapshot. This avoids any cache lookups and keeps the same GPU resources.

4. **Integrate with editor:**  
   - Ensure any code that adds/removes entities or changes render components marks the scene dirty (or bumps the revision).  
   - `CaptureRenderSnapshot` stays per-frame; the **build from snapshot** step runs only when dirty.

5. **Validate:** With a static model in the scene, confirm (e.g. via log) that Build runs only once (or after an edit); FPS remains high.

**Files to touch:**  
`app/editor/EditorSceneBuilder.cpp` (and `.h`), possibly `api/world/World` or snapshot type for revision/hash.

**Success criteria:** Scene drawable list is rebuilt only when scene content or references change; per-frame work no longer includes full iteration and resolution for every entity.

---

## Phase 3: Async Mesh Load + Deferred GPU Upload

**Problem:** First time a mesh is needed, load + parse + `ImportMeshToDescriptor` + `CreateFromDescriptor` (GPU upload) run synchronously on the main thread, causing a visible hitch.

**Goal:** Load and parse mesh on a background thread; upload to GPU on the main thread in a controlled way (deferred queue or next frame), or use a staging buffer and async transfer if the RHI supports it.

**Tasks (in order):**

1. **Introduce an async load API for meshes:**  
   - e.g. `AssetRegistry::LoadMeshAsync(id, callback)` or `AssetManager::LoadMeshAsync(guid, callback)` that runs file read + `MeshReader::Read` + `ImportMeshToDescriptor` on a worker thread.  
   - Callback receives a `MeshDescriptor` (or shared_ptr to CPU-side data).  
   - Main thread must not use this descriptor for GPU upload from the worker.

2. **Deferred GPU upload queue (main thread):**  
   - When the async callback runs (or is dispatched to main thread), push a “pending upload” job: descriptor + asset id (and optional callback when ready).  
   - At a safe point each frame (e.g. start of render, or end of update), process N pending uploads (e.g. 1–2 per frame to spread cost, or all if budget allows).  
   - For each: call `CreateFromDescriptor(device, desc)`, store in resolver cache, notify any waiters (e.g. placeholders).

3. **Integrate with ModelLoader / EditorSceneBuilder:**  
   - When a mesh is required and not in cache, kick async load and (optionally) show a placeholder (e.g. unit cube or nothing) until the deferred upload completes.  
   - Once uploaded, next frame’s Build (or dirty rebuild) will see the mesh in cache and use it.

4. **Optional: preload on scene open:**  
   - When opening a scene or when `BindSceneAssets` runs, kick async loads for all referenced meshes so that by the time the user sees the scene, many are already in cache or in the upload queue.

5. **Validate:** Load a heavy model; confirm no main-thread stall (smooth frame times), and model appears within a frame or two after load completes.

**Files to touch:**  
`api/asset/AssetRegistry.cpp` (and loaders), `eyelib/asset/runtime/asset_manager.cpp`, `app/editor/EditorSceneBuilder.cpp`, resolver (e.g. EditorApp GPU cache) for pending-upload queue and placeholder handling.

**Success criteria:** No frame hitch when a new mesh is first needed; load and parse are off the main thread; GPU upload is amortized or deferred.

---

## Phase 4: GPU Instancing for Same Mesh + Material

**Problem:** Each drawable is one draw call (one mesh bind, one draw). Many entities sharing the same mesh and material still produce N draw calls.

**Goal:** Batch drawables that share the same mesh and material (and same shader variant); issue one instanced draw (e.g. `DrawIndexedInstanced`) per batch, with per-instance transforms (and optional per-instance params) from a buffer.

**Tasks (in order):**

1. **Instance buffer or SSBO:**  
   - Allocate a GPU buffer for per-instance data (e.g. `mat4 model` or `mat4 model + mat4 prevModel`).  
   - Each frame (or when dirty), fill it with transforms for all instances in a batch.  
   - Bind as vertex buffer (instanced) or as SSBO and use `gl_InstanceID` / equivalent in the shader.

2. **Batch by (mesh, material) in the render path:**  
   - When building the draw list (or in GBufferPass / Forward pass), group drawables by `(mesh, material)` (or by `ShaderKey` that already encodes material).  
   - For each group with more than one drawable: collect transforms, upload to instance buffer, bind mesh and material once, call `DrawIndexedInstanced(count)`.  
   - For groups of one, keep current path (single draw) or still use instancing with count 1.

3. **Shader and pipeline:**  
   - Use or add an instanced variant (e.g. existing `gbuffer_instanced` / forward instanced) that reads per-instance transform from the buffer.  
   - Ensure vertex shader uses `gl_InstanceID` to fetch the right transform; no need to change the fragment shader.

4. **Shadow pass:**  
   - Apply the same batching for shadow map rendering: batch by mesh, issue `DrawIndexedInstanced` with the same instance buffer (or a shadow-specific one).

5. **Validate:** Place many copies of the same model in the scene; confirm draw call count drops (e.g. one or few draws per mesh+material) and FPS improves.

**Files to touch:**  
`eyelib/render/passes/GBufferPass.cpp`, `eyelib/render/renderer/RenderPipeline.cpp` (forward opaque), `eyelib/render/shadows/ShadowPass.cpp`, instance buffer management (e.g. in pipeline or pass), and instanced shaders if not already used.

**Success criteria:** Draw call count scales with unique (mesh, material) pairs, not entity count; FPS scales well with many instances.

---

## Phase 5: Mesh Descriptor — Zero-Copy or Single-Buffer Path (Optional)

**Problem:** `ImportMeshToDescriptor` does a full copy of vertices and indices from `ImportMesh` to `MeshDescriptor`. Extra allocation and copy on load.

**Goal:** Either build `MeshDescriptor` in a form that can be used without a second full copy, or produce a single contiguous buffer (e.g. for upload) directly from the parsed format.

**Tasks (in order):**

1. **Option A — In-place or view:**  
   - If `MeshDescriptor` can hold pointers/spans to memory owned by `ImportMesh` (or a shared buffer), avoid copying; ensure lifetime is clear (e.g. descriptor used only until GPU upload completes).  
   - Option B: MeshReader writes directly into a buffer layout that matches what the GPU upload expects (e.g. interleaved vertex struct), so one alloc and one upload.

2. **Refactor `GetMeshDescriptor` / `CreateFromDescriptor` path:**  
   - Ensure we don’t hold duplicate vertex/index data in both ImportMesh and MeshDescriptor longer than necessary.  
   - After GPU upload, release CPU-side data if not needed for anything else (e.g. LOD, collision).

3. **Validate:** Load a large mesh; confirm lower peak memory and (if measurable) slightly faster load.

**Files to touch:**  
`api/asset/AssetRegistry.cpp` (ImportMeshToDescriptor), `eyelib/render/MeshDescriptor.h`, `eyelib/asset/readers/mesh_reader.cpp`, and upload path in `Mesh::CreateFromDescriptor`.

**Success criteria:** No redundant full copy of vertex/index data; load path is cleaner and optionally lighter on memory.

---

## Phase 6: Consolidation and Polish

**Goal:** Single source of truth, clear ownership, and no redundant per-frame work anywhere.

**Tasks (in order):**

1. **Audit per-frame work:**  
   - List everything that runs every frame in the editor render path (Build, Compile, ForEachOpaqueDrawable, shadow pass, etc.).  
   - Ensure each is either necessary (e.g. Execute, actual draw) or guarded by a dirty flag / revision.

2. **Unify scene representation (optional):**  
   - If both `snapshot.renderables` and `snapshot.meshRenderers` are used, consider a single representation (e.g. “renderables” only) so Build and batching logic don’t have to handle two lists.

3. **LOD and culling:**  
   - Ensure LOD selection and frustum culling run only when needed (e.g. when scene or camera changes), and don’t rebuild the whole world every frame.

4. **Profiling and metrics:**  
   - Add simple timers or debug overlays (e.g. Build time, Compile time, draw call count, instance count) so future regressions are visible.

5. **Documentation:**  
   - Update `documentation/refractor/Render_refractor.md` or this roadmap with “Implemented” notes and any deviations (e.g. “Frame graph: compile on resize only”).

**Success criteria:** No redundant work per frame; clear ownership; easy to profile and maintain.

---

## Implementation Order Summary

| Order | Phase | Main outcome |
|-------|------|----------------|
| 1 | Frame graph compile once | No per-frame DAG/Compile; only Execute each frame. |
| 2 | Incremental scene build | No full Build every frame; rebuild only when scene is dirty. |
| 3 | Async mesh load + deferred upload | No load hitch; smooth first appearance of models. |
| 4 | GPU instancing | Draw calls scale with batches, not entity count. |
| 5 | Zero-copy/single-buffer mesh (optional) | Lower memory and cleaner load path. |
| 6 | Consolidation and polish | Single source of truth; measurable; maintainable. |

Do **Phase 1** first (biggest per-frame win), then **Phase 2** (removes remaining O(entities) rebuild). **Phase 3** fixes the one-time hitch. **Phase 4** scales rendering with instance count. **Phase 5** is optional optimization; **Phase 6** locks in cleanliness and observability.

---

## Implemented

Update this section as each phase is completed. Use **Done** only when the implementation is professional (no workarounds) and meets the phase success criteria.

| Phase | Status | Notes |
|-------|--------|--------|
| 1 — Frame graph compile once | Done | Compile only when needsCompile \|\| size/mode/stereo changed; executors use m_currentView and m_lastGpuCullDrawListCount; Resize() sets m_frameGraphNeedsCompile. |
| 2 — Incremental scene build | In progress | EditorSceneBuilder now uses incremental per-entity static drawable cache (mesh/material/submesh), cached asset path/material resolution, and stale-cache pruning; transform/material params still refreshed per frame for correctness. |
| 3 — Async mesh load + deferred upload | Not started | |
| 4 — GPU instancing | Not started | |
| 5 — Zero-copy / single-buffer mesh (optional) | Not started | |
| 6 — Consolidation and polish | Not started | |

---

*Document: Pipeline efficiency roadmap — sequential implementation plan.*  
*Location: `documentation/refractor/Pipeline_efficiency_roadmap.md`.*
