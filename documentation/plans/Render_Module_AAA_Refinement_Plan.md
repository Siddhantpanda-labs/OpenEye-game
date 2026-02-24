# Render Module AAA Refinement Plan

**Goal:** Refine the existing render module and RHI into an **architecturally mature, AAA-ready** system without rewrites. Prioritize **structural safety** and **Vulkan readiness** over feature expansion. Discipline now avoids 6+ months of invasive refactors later.

**Scope:** `eyelib/render/` (RHI, frame graph, pipeline, passes, resources), `eyelib/fluids/`, `eyelib/debug/` (Profiler). API and game/render thread model unchanged.

**References:**  
- `RHI_And_Graphics_Backend_Plan.md` — RHI design and migration status  
- `Complete_rendermodule_plan.md` — Ultimate GPU architecture vision  
- `Render_Thread_Unified_Scene_Plan.md` — Thread model and presentation  

**Status:** OpenGL is the default and only target backend (no Vulkan planned). Remaining work is tracked in **§1 Current Enhancement Plan** below.

---

## 1. Current Enhancement Plan (Implementation Tracker)

Remaining work to reach AAA-level render module (per `Complete_rendermodule_plan.md`). **OpenGL only**; no Vulkan backend. Where OpenGL has no native support (e.g. deferred command buffers), the OpenGL backend implements a workaround (e.g. record to CPU, replay on submit).

| # | Enhancement | Priority | Status | Notes |
|---|-------------|----------|--------|-------|
| E1 | **Deferred command buffer model** | P0 | ☑ Done | RHI: BeginCommandBuffer / EndCommandBuffer / Submit (and optional Wait). OpenGL backend: emulate by recording to CPU-side command stream, replay on Submit() on render thread. |
| E2 | **Multithreaded command recording** | P0 | ☑ Done | Multiple command lists record in parallel (e.g. from jobs); single-thread Submit in order. OpenGL backend: record only; execution serial. One list per worker; CreateCommandList/Submit documented. |
| E3 | **Frame graph: transient pooling & aliasing** | P0 | ☑ Done | Pool transient resources; memory aliasing for non-overlapping lifetimes. FGTransientKey pool; Acquire/Release; greedy alias slots per (w,h,format). |
| E4 | **GPU-driven adoption** | P1 | ☑ Done | GPU Cull pass (compute → indirect buffer) in frame graph; GpuCullPass + cull_indirect.comp; barrier for IndirectDraw. Buffer available for MultiDrawIndexedIndirect in any pass. |
| E5 | **Shader system** | P2 | ☑ Done | Reflection (ParseShaderReflection, GetReflection), keywords (Load with defines), pipeline cache (Save/Load binary, OpenGL Get/CreateFromBinary), hot reload (Reload()). |
| E6 | **Post-processing: graph-driven** | P2 | ☑ Done | AddPostPassesToFrameGraph: ToneMap + FXAA as FG passes. Deferred path uses FG post; forward keeps monolithic ApplyPostProcessing. |
| E7 | **Streaming / LOD hooks** | P3 | ☑ Done | StreamingHooks: budget getter, residency query/set, RequestTextureStream, RequestMeshLOD, RequestAsyncBufferUpload (default sync). Optional / engine scope. |
| E8 | **GPU-driven consumer adoption** | P1 | ☑ Done | GBufferPass::RenderIndirect; pipeline flag SetUseGpuDrivenGBuffer; scene-fed draw list (A2) + frustum cull (A1); single path chosen by capability + data (C1). Fallback: per-draw path. See §8 Adoption Plan. |
| E9 | **Bindless adoption** | P2 | ☑ Done | Pipeline creates IBindlessHeap when supportsBindless; GetOrAllocateBindlessHandle caches handles. GBufferPass: when heap + getter set, uses BindBindlessHeap and SetHandle64 per texture; fallback to bind-per-unit. RHI: SetUniformHandle64 (OpenGL glProgramUniformHandleui64ARB). See §8 Adoption Plan. |

**Status key:** ☐ Not started | ◐ In progress | ☑ Done. Update this table as work completes.

### 1.1 Current work & open problem (GPU Cull / GL 1282)

**How far we've come**

- **GL 1282 (GL_INVALID_OPERATION — "Wrong component type or count")** was reported in the GPU Cull compute pass when using the frame graph (Deferred/Forward), at the point we bound SSBO slot 1 (draw list).
- **Root cause identified:** We were binding a buffer to SSBO slot 1 at execute time without checking whether the linked program actually had an active block at that slot. If `DrawListIn` was optimized out or missing, binding to slot 1 is invalid; the driver was correct to flag it.
- **Fixes applied:**
  1. **Program-interface invariant:** Only bind SSBO slot 1 when the compute pipeline has an active `DrawListIn` block. RHI: `IDevice::ComputePipelineHasStorageBlock(pipeline, "DrawListIn")` (default `true`); OpenGL: `glGetProgramResourceIndex(..., GL_SHADER_STORAGE_BLOCK, "DrawListIn")`; **GpuCullPass::Execute** binds slot 1 only when `hasDrawListBlock && useDrawList && effectiveCount > 0`. Never bind to a resource slot that does not exist in the linked program.
  2. **Pipeline creation log:** At compute pipeline create, log a warning when `DrawListIn` is missing so we have an audit trail.
  3. **Earlier hardening (already in place):** View-proj for the cull shader is provided via a UBO (binding 3) instead of a mat4 uniform; single combined buffer holds both indirect region and draw-list region, bound at slot 0 and slot 1 with different offsets to avoid a second buffer object.

**Current faced problem**

- **Status:** Invariant is now enforced at execution time. If GL 1282 disappears, the bug was binding to a non-existent slot and the fix is complete. If 1282 persists after this change, next steps are: verify actual block binding index (`glGetProgramResourceiv` GL_BUFFER_BINDING), try two separate buffers (no aliasing), and/or treat as driver-specific and consider a targeted workaround (e.g. separate buffers, driver-version check, or filtering that specific error for that GPU only after proof).
- **Lesson:** Enforce program-interface invariants at execution time; do not assume a block exists because the shader source declares it (optimizer may remove it).

---

## 8. Adoption Plan (Full Complete & Generic)

RHI and OpenGL backend already expose **bindless**, **indirect draw**, and **GPU cull**. This section tracks **render-module adoption** so the pipeline actually uses them (capability-driven, with fallbacks).

### Phase A — GPU-driven draw (consumer path)

| Step | What | Generic |
|------|------|--------|
| A1 | **Real cull in compute** | ☑ Done. `cull_indirect.comp` reads draw list (GpuCullDrawEntry: AABB + draw params), derives frustum from u_ViewProj, writes one indirect command per input (visible = full command, culled = instanceCount 0). SetDrawList/SetViewProj on GpuCullPass. Legacy: u_NumDraws=0 → one fixed quad. Gate on supportsCompute + supportsMultiDrawIndirect. |
| A2 | **Feed GpuCullPass from scene** | ☑ Done. RenderPipeline builds GpuCullDrawEntry[] from ForEachOpaqueDrawable (world AABB via mesh AABB + transform, draw params from mesh); uploads to m_gpuCullDrawListBuffer; SetDrawList + SetViewProj before GPU Cull pass. GBuffer uses per-draw path when draw list present (multi-mesh); uses RenderIndirect only when no scene drawables (single quad). |
| A3 | **Consumer pass** | ☑ Done. GBuffer has path: when indirect buffer provided (and no scene draw list) → RenderIndirect (pipeline, VB/IB, MultiDrawIndexedIndirect); else per-draw loop. |
| A4 | **Fallback** | ☑ Done. When indirect path unavailable or scene has drawables, same pass uses per-draw path. Single pass, two code paths by capability + data. |

### Phase B — Bindless adoption

| Step | What | Generic |
|------|------|--------|
| B1 | **Bindless heap lifecycle** | ☑ Done. Pipeline creates heap in Initialize when supportsBindless; GetOrAllocateBindlessHandle(ITexture*) caches handles. |
| B2 | **Bindless in one pass** | ☑ Done. GBufferPass: when m_bindlessHeap and m_bindlessGetHandle set, BindBindlessHeap(cmd) and SetHandle64 per texture; same shader (sampler2D set with handle via SetUniformHandle64). |
| B3 | **Shader variants** | N/A. Same shader: OpenGL accepts handle for sampler2D uniform via glProgramUniformHandleui64ARB; no BINDLESS #ifdef needed. Fallback when !supportsBindless: bind-per-unit path. |

### Phase C — Unified pipeline

| Step | What |
|------|------|
| C1 | **Unified geometry path** | Done. RenderOpaqueGeometryToGBuffer(cmd, view, cam, gpuCullDrawListCount) is the single entry point; chooses CPU submit (per-draw) or GPU submit (indirect) by capability + data. GBuffer pass calls it. |
| C2 | **Unified material path** | Done. Material and MaterialInstance expose GetTextureRHI(name) and BindTextureToSlot(name, unit); GBufferPass uses them for bindless (handle) or bind-per-slot. |
| C3 | **Plan table** | Done. §1 and §7 updated; E8/E9 marked complete. |

---

## 2. Strategic Evaluation

### 2.1 What Is Strong (Already Done Right)

| Strength | Why it matters |
|----------|----------------|
| **Render module is backend-agnostic** | No GL outside `rhi/OpenGL/`. Avoids the biggest architectural trap. |
| **PSO-style pipelines** | `PipelineDesc` (shader + blend + depth + raster + layout) is the correct, professional direction. |
| **Explicit resource model** | Create/Destroy for buffers, textures, render targets, pipelines. |
| **Frame graph exists** | Pass registration, read/write declaration, auto barriers, state tracking. |
| **Barriers in RHI** | `BarrierBuffer` / `BarrierTexture`; RHIUtils provide barrier helpers. |

### 2.2 Structural Gaps (Address via §1)

| Gap | Addressed by |
|-----|--------------|
| Single immediate command list | E1–E2 (deferred buffers, multithreaded recording). |
| Frame graph without transient pooling | E3. |
| GPU-driven engine adoption | E4. |

### 2.3 Architectural Risks

| Risk | Long-term direction |
|------|----------------------|
| **Global default device** | Inject device explicitly; reduce `GetDefaultDevice()`. |
| **Native handle import** | Import only via RHI handle (`ITexture*`), not native id. |

---

## 3. What Not to Do (Yet)

- **Do not** add GPU-driven passes before the command buffer model (E1–E2) is in place.
- **Do not** refactor PostProcessor into full graph-driven effects until frame graph enhancements are stable; it can follow E3.
- **Do not** remove `GetDefaultDevice()` abruptly; reduce usage over time.
- **Vulkan:** Not in scope; OpenGL-only backend.
- Procedural/
    Noise/
    TerrainGenerator/
    BiomeSystem/
    ScatterSystem/
    RuleEvaluator/
    ChunkGenerator/

---

## 4. Success Criteria (Overall)

- **Backend-agnostic:** No GL outside RHI backends.
- **Capability-driven:** Use `RHICapabilities`; no backend branching.
- **Explicit control:** Command buffer model (Begin/End/Submit), barriers, resource state; frame graph enforces barriers.
- **Descriptor/layout driven:** Pipeline layout and descriptor sets; slot binding is implementation detail.
- **Scalable:** GPU-driven and bindless in RHI; render module adopts via §1 (E1–E9) and §8 Adoption Plan.

---

## 5. Decision: Stabilize First vs Features First

**Recommendation:** **Stabilize architecture first.** Complete E1–E3 (command buffer model, multithreading, frame graph pooling) before pushing GPU-driven or post-processing features. Discipline now avoids later refactors.

---

## 6. Document History

- **Created:** Refinement plan; references RHI_And_Graphics_Backend_Plan, Complete_rendermodule_plan, Render_Thread_Unified_Scene_Plan.
- **Updated:** “What’s done” (§1); Removed completed-phase content; plan focuses on **Current Enhancement Plan** (§1) and strategic evaluation.
- **Updated:** Added **§8 Adoption Plan** (Phase A GPU-driven consumer, Phase B bindless, Phase C unified); added **E8**, **E9**; refreshed §7.
- **Updated:** E8 marked done; §7 LEVEL 4/7 marked done; §8 A3/A4 marked done, C3 done; C1 in progress.
- **Updated:** Added §1.1 Current work & open problem (GPU Cull / GL 1282): progress (UBO view-proj, single combined buffer, program-interface invariant — only bind SSBO slot 1 when DrawListIn exists; ComputePipelineHasStorageBlock; log at pipeline create); current status and next steps if 1282 persists.

---

## 7. What's Done vs. Left (per Complete_rendermodule_plan)

| Level / Area | Status | Notes |
|--------------|--------|-------|
| **RHI explicit (LEVEL 0–1)** | ☑ Done | Command buffer model (E1–E2); explicit control. |
| **Frame graph (LEVEL 2)** | ☑ Done | Auto barriers, state, lifetime, transient pooling (E3). |
| **OpenGL backend** | Complete | Full RHI; see `Opengl_case.md`. Workarounds (e.g. deferred buffers) in backend. |
| **GPU-driven (LEVEL 4)** | ☑ Done | E4 + E8: cull pass, scene-fed draw list, frustum cull, consumer path (RenderIndirect or per-draw). |
| **Multithreading (LEVEL 5)** | ☑ Done | E2: multithreaded command recording. |
| **Shader system (LEVEL 6)** | ☑ Done | E5: reflection, keywords, cache, hot reload. |
| **Bindless (LEVEL 7)** | ☑ Done | E9: heap, handle cache, GBufferPass bindless path; fallback to bind-per-unit. |
| **Vulkan backend** | Out of scope | OpenGL-only; not planned. |
| **Streaming / LOD (LEVEL 8+)** | ☑ Done | E7: hooks. |

**Summary:** E1–E9 and §8 Phase A/B complete. **Phase C** (C1 unified geometry, C2 unified material, C3 plan table) complete. See **§8 Adoption Plan**.



# More - 1️⃣ Multiple Cameras in a Single-Threaded OpenGL Engine

A camera in OpenGL is just:

* A **view matrix**
* A **projection matrix**
* Maybe some viewport/scissor settings

That’s it.

You can render as many cameras as you want in a single thread by doing:

```
for each camera:
    set viewport
    upload view matrix
    upload projection matrix
    render scene
```

That’s completely valid.

### Example Scenarios

* Split screen
* Minimap camera
* Reflection camera
* Shadow map camera
* Security monitor screens
* Editor viewport + game viewport

All of these are done in a **single render thread** in many engines.

No threading required.

---

# 2️⃣ What Actually Changes When Using Multiple Cameras?

What changes is:

* You render the scene multiple times
* Each camera = one render pass
* GPU cost increases
* CPU draw call cost increases

But threading has nothing to do with correctness.

Threading is about performance scaling, not capability.

---

# 3️⃣ What About VR?

VR is where things become interesting.

In VR, you have:

* Left eye camera
* Right eye camera
* Slightly offset view matrices
* Same scene rendered twice
* Strict 90Hz+ timing requirement

Still:

You can render VR in a single thread.

Most engines do exactly this:

```
for each eye:
    bind eye framebuffer
    set view/projection
    render scene
submit to VR runtime
```

No threading required.

---

# 4️⃣ When Does Threading Actually Become Necessary?

Threading becomes useful for:

### ✔ Asset loading

### ✔ Physics simulation

### ✔ AI

### ✔ Streaming large worlds

### ✔ Command buffer building (advanced engines)

### ✔ Reducing CPU bottlenecks in huge scenes

Not for “multiple cameras”.

---

# 5️⃣ Important: OpenGL and Threads

OpenGL contexts are **not automatically thread-safe**.

If you later decide to multithread rendering:

* You must manage contexts
* You cannot call GL from multiple threads on same context
* You need shared contexts or command queues

This becomes complex fast.

So staying single-threaded at first is actually smart.

---

# 6️⃣ Brutally Honest Advice

If your engine:

* Is still growing
* Is OpenGL-based
* Is learning-focused

Then:

> Focus on architecture before threading.

Threading too early will destroy your clarity.

Multiple cameras is not a threading problem.
It is a render-pass architecture problem.

---

# 7️⃣ Architecture You Should Think About

Instead of asking:

> “Do I need threading?”

Ask:

> “Is my renderer structured as a render graph or pass system?”

You want something like:

```
RenderScene(Camera& cam, Framebuffer target);
```

Then your engine becomes camera-agnostic.

VR becomes trivial.
Split-screen becomes trivial.
Editor view becomes trivial.

Threading becomes optional.

---

# 8️⃣ Forward-Thinking Consideration (Since You Like Engine Work)

If you plan VR later:

Design your renderer like this:

* Scene update phase
* Visibility phase
* Render submission phase
* Presentation phase

Keep them conceptually separated even if they run in one thread.

That design will scale when you eventually introduce:

* Multithreaded command building
* Vulkan migration
* Stereo instanced rendering

---

# 9️⃣ One Subtle VR Optimization

Later, when doing VR:

Look into:

* **Single-pass stereo**
* **Instanced rendering**
* Using `gl_ViewID` (if moving to modern APIs)

But again — that’s optimization, not requirement.

---

# Final Verdict

| Feature             | Needs Threading? |
| ------------------- | ---------------- |
| Multiple cameras    | ❌ No             |
| Split screen        | ❌ No             |
| Minimap             | ❌ No             |
| Shadow maps         | ❌ No             |
| VR rendering        | ❌ No             |
| Performance scaling | ⚠ Maybe later    |

---

If you want, tell me:

* Are you using forward or deferred rendering?
* Is your engine ECS-based?
* What OpenGL version?

I’ll tell you how to structure it so adding VR later doesn’t force a rewrite.
