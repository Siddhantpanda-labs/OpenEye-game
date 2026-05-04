# Render fix / debug track — deferred path (black screen)

## Problem

**Symptom:** With `RenderMode::Deferred`, the rendering sandbox (or equivalent) shows a **black** image even after the `DeviceVK::GetResourceSetLayout` off-by-one fix (`RasterDeferredLightingData` must be reachable).

**Goal:** Localize where the GPU path breaks (schedule vs G-buffer vs deferred lighting vs blit/present) using **distinct clear colors** and **bounded `[DeferredDebug]` logs**, then remove temporary instrumentation once the root cause is fixed.

---

## Localization snapshot — how close we are (not “completely unknown”)

**We are not in a blank unknown space.** A large outer ring of the pipeline has been **checked in code** and is either **correct** or **already fixed** (pass order, blackboard RTs, shared depth identity, deferred descriptor cache, depth-only SRV, depth sample barriers, UV math for fullscreen triangle, buffer upload barriers, dynamic depth refresh). That work **shrinks** where a bug can hide.

**What is left is a small inner region — one primary fork:**

| If this is true… | Then the bug hides in… |
|-------------------|-------------------------|
| **G-buffer MRT0 / depth images still look like clear** (or uniform debug tint) **at pixels where the cube should be** | **`GBufferPass` raster path**: clip / projection / depth test / viewport, or “draw recorded but **no fragment** updates attachments” (matrix, pipeline static state without ext. dynamic, etc.). **Not** the blit, **not** “wrong texture pointer” unless descriptors for **G-buffer draw** were wrong (we use frame/pass sets keyed on buffers — lower risk than deferred texture cache). |
| **G-buffer images show the cube**, but **deferred fullscreen** still shows clear / wrong | **`DeferredLightingPass` + barriers + sampling** (we already fixed several real issues here; any remainder is **edge** sync or a remaining descriptor/layout bug). |

So the problem is **not** “anywhere in rendering.” It is **almost certainly** either **(A) nothing ever paints the G-buffer where you think**, or **(B) deferred read path** if (A) is false. **(A)** is the default interpretation when **debug albedo = clear-like** and **debug depth ≈ 1** everywhere.

**Confidence (informal):** ~**70–85%** of the *system* is ruled in or fixed by review; the **residual uncertainty** is **~15–30%** concentrated in **per-pixel G-buffer behavior**.

**2026-05-04 — RenderDoc (narrowing closed for A vs B on color):** G-buffer **MRT0 / albedo** RT shows a **flat field** matching the **debug clear** (`deferred_debug::kGBufferClearRgba` in `DeferredDebugVis.h` — purple / reads as dark magenta in the texture viewer). That **confirms fork (A)**: the mesh is **not** updating that attachment (no winning fragments over the cube), not “G-buffer fine, deferred sampling only wrong.” Treat **deferred-only** hypotheses as **secondary** until MRT0 shows real albedo.

**Where the bug lives in code (no RenderDoc required — read these):**

| Layer | File / symbol | What to verify |
|--------|----------------|----------------|
| **Clip / matrices** | `gbuffer_static.vert` → `gl_Position = uPerView.uProjection * uPerView.uView * worldPos` | Same convention as `RasterFeature::OnUpload`: `viewProjection = projection * view` (column vectors). Wrong order or missing `AdjustCameraForBackend` → full clip or nonsense NDC. |
| **Per-view upload** | `RasterFeature.cpp` `OnUpload` | `TryGetCamera()` null → identity matrices in `PerViewUniformStd140` (`FrameViewUniforms.h`); combined with Vulkan depth clip can yield **no coverage** until a valid camera is uploaded. |
| **Per-object + push index** | `RenderResources::UploadPerObjectBufferFromDrawList`, `GBufferPass::Execute` `SetPushConstants(..., &i)` | Row `i` must match `drawList->items[i]` (it does by construction); wrong SSBO layout would garble `row.model` (guarded by `static_assert` on `PerObjectGpuStd430`). |
| **Draws actually issued** | `GBufferPass.cpp` loop | `layer != DrawLayer::Opaque` skips; `TryResolveMesh` fail skips; `indexCount == 0` / empty submeshes → **IA valid in RD mesh view but zero fragments**. |
| **Depth vs clear** | `GBufferPass::Execute` + `makeGBufferOpaqueBaselineSpec()` | Clear depth `1.0`, compare **Less**, depth write on — if **dynamic depth** is off-device, static pipeline must still declare D24S8 (`PipelineVK.cpp` / `SelectRenderingFormats` from `attachmentFormats`). |

---

## Process to follow

1. **Confirm build** runs with `OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS` enabled (default `1` in `DeferredDebugVis.h`).
2. **Run** `OpenEyeRenderingSandbox` (Debug) and note:
   - **On-screen tint** (swapchain is sRGB/tone-mapped; interpret hues qualitatively).
   - **Console lines** prefixed with `[DeferredDebug]`.
3. **RenderDoc** (optional): inspect **G-buffer MRTs**, **Main HDR** after deferred pass, **swapchain** after blit.
4. **Interpret** using the color legend below; narrow to one stage, then fix product code (not the debug legend).
5. **Revert debug** when done:
   - Set `#define OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS 0` in `EngineSource/eyelib/rendering/core/DeferredDebugVis.h`, **or** remove the macro blocks / header include from passes and `RasterFeature`.
   - Remove or archive the “Debug changes (temporary)” section entries below.

---

## Debug legend (clear colors when flag = 1)

| Step | Where | Clear RGB (linear-ish) | Meaning |
|------|--------|-------------------------|---------|
| **A** | `GBufferPass` — all G-buffer color attachments | **Purple** `(0.75, 0, 0.85)` | G-buffer pass began; visible in RenderDoc on G-buffer RTs before draws overwrite. |
| **B** | `DeferredLightingPass` — Main HDR color | **Orange** `(0.95, 0.55, 0.05)` | Deferred lighting pass cleared HDR before fullscreen draw; if final window shows strong orange, HDR saw this clear then something overwrote or blit sampled wrong. |

**Logs**

- `[DeferredDebug] RasterFeature deferred passes NOT scheduled` — pipelines or inputs missing; G-buffer / deferred never added to graph.
- `[DeferredDebug] GBufferPass::Execute` — periodic; proves G-buffer Execute entered.
- `[DeferredDebug] DeferredLightingPass early: …` — which guard fired (hdr/gbuffer, UBOs, descriptor sets, DescribeRenderAttachmentOps).
- `[DeferredDebug] DeferredLightingPass: vkCmdDraw(3) fullscreen` — periodic; proves path reached fullscreen draw.

---

## Debug changes (temporary — revert after cause fixed)

| Date | Change | Revert by |
|------|--------|-----------|
| 2026-05-03 | Added `EngineSource/eyelib/rendering/core/DeferredDebugVis.h` with `OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS` and `deferred_debug::kGBufferClearRgba` / `kHdrDeferredClearRgba`. | Delete header or set macro to `0` and remove includes. |
| 2026-05-03 | `GBufferPass.cpp`: debug clears + throttled `GBufferPass::Execute` log. | Restore fixed black clear `{0,0,0,1}`; remove `#if` log block. |
| 2026-05-03 | `DeferredLightingPass.cpp`: debug HDR clear + early-exit logs + throttled pre-draw log. | Restore `{0.1f,0.15f,0.2f,1}` clear; remove `#if` log blocks. |
| 2026-05-03 | `RasterFeature.cpp`: log when deferred passes are **not** scheduled (pipelines / mesh / draw list). | Remove `#if OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS` block and include. |
| 2026-05-03 | `deferred_lighting_analytic.frag`: added `OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE` probe (`1=depth`, `2=normal`, `3=albedo`) for step-by-step isolation. | Set mode to `0` (or remove macro block) after diagnosis. |
| 2026-05-03 | `GBufferPass.cpp`: added runtime counters (`opaque`, `resolved`, `resolveFail`, draw-call counts) + bounded zero-draw warning. | Remove temporary `[DeferredDebug] GBufferPass stats/0 draws` logs after root cause is fixed. |
| 2026-05-03 | `GBufferPass.cpp`: added first-resolved-mesh dump (`vb`, `ib`, `submeshes`, `totalIndexCount`) + zero-index-draw warning. | Remove temporary `[DeferredDebug] GBufferPass mesh[...]` and zero-index warnings after diagnosis. |

**Prior permanent fixes (not temporary):**

1. `DeviceVK::GetResourceSetLayout` — use `sizeof(m_resourceSetLayouts) / sizeof(...)` instead of `i < 11` so `RasterDeferredLightingData` resolves. Do **not** revert that.
2. **D24S8 sampled depth (VUID-VkDescriptorImageInfo-imageView-01976):** `TextureVK` creates an extra **depth-only** `VkImageView` (`m_depthSrvImageView`); `GetVkImageViewForShaderSamplerBinding()` selects it for `DescriptorSetVK::UpdateTexture`, bindless, and `VulkanBindingUtils`. The primary `m_imageView` stays **depth+stencil** for attachments and barriers. Do **not** revert without replacing with an equivalent spec-correct path.
3. **Deferred lighting sampled wrong depth:** `DeferredLightingPass` must bind **`GBufferRenderTarget` depth**, not `MainHdrRenderTarget` depth (HDR depth is not written during G-buffer). Wrong binding left depth at clear (1.0) so the analytic shader always took the sky branch.
4. **Stale deferred lighting descriptors:** `DeferredLightingPass` cached pass descriptors with a key that omitted **G-buffer color texture pointers**. After resize / `RenderResources::EnsureAllocated`, new MRT images could be created while shared depth stayed the same → descriptors still sampled **old/freed** color attachments (flat / garbage). Fix: fold `GetColorTexture(0..2)` (and depth) into `dataKey`, or refresh textures every frame.
5. **`DepthRead` must synchronize shader sampling:** `ResourceState::DepthRead` in `EngineSource/eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h` includes **`VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT`** and **`VK_ACCESS_SHADER_READ_BIT`** (in addition to depth-test stages/access) so `texture(tDepth)` after G-buffer is valid. **`DeviceVKPresentation.cpp`** depth layout helper updated to match.

---

## Hypothesis narrowing — clear visible, RenderDoc shows draw + mesh, final image has no silhouette

**How far we’ve narrowed (code + symptoms, no architecture change to `eyelib/rendering`):**

| Area | Confidence | Notes |
|------|------------|--------|
| Schedule / pass order | **High — OK in code** | `RasterFeature::OnSchedulePasses`: deferred mode adds **GBuffer → DeferredLighting → Blit** (`RasterFeature.cpp`). |
| Wrong HDR vs G-buffer **depth** binding | **High — fixed** | `DeferredLightingPass` binds `gbuffer->GetDepthTexture()`; `RenderResources` shares one `MainDepth` with both RTs. |
| Stale **deferred** descriptors (colors/depth) | **High — fixed** | `dataKey` includes `GetColorTexture(0..2)` + depth (`DeferredLightingPass.cpp`). |
| Depth **sampled** vs barrier stages (FS `texture()`) | **High — fixed** | `ResourceState::DepthRead` now includes **fragment shader + `SHADER_READ`** in `eyelib/rendering/.../VulkanEnums.h` (and matching `DeviceVKPresentation.cpp` helper). |
| G-buffer **color** `RT → ShaderResource` barriers | **Medium — looks OK** | `DeferredLightingPass::CollectTextureBarriers` pushes MRT0–2 to `ShaderResource`; `VulkanEnums.h` maps that to FS/VS/CS + `SHADER_READ` + `SHADER_READ_ONLY_OPTIMAL`. |
| **Fragments never shade / never pass depth** | **Active — fork (A) confirmed on MRT0** | RenderDoc: G-buffer **albedo RT stays clear color** → no FS commits (clip, depth, or zero-area draw). Next fixes target **`gbuffer_static.vert` + camera upload + `GBufferPass` draw loop**, not deferred sampling first. |
| **Dual asset roots / stale SPIR-V** | **Out of scope** | User asked **not** to track this angle; focus stays on runtime wiring + raster. |
| **Architectural difference vs old render** | **Documented — not a bug** | Old `eyelib/render` deferred uses a **world-position G-buffer** + **`v_TexCoord` ∈ [0,1]** (`assets/shaders/vulkan/passes/deferred/lighting.frag`); `eyelib/rendering` keeps **packed MRTs + depth + inv(viewProj)** and **fullscreen triangle `vUv*0.5`**. We keep the new design; old code is only a **reference for “how it worked there.”** |

---

### Evaluation matrix (each hypothesis, code-only where possible)

| # | Hypothesis | Verdict | Evidence in `eyelib/rendering` | Deprecated `eyelib/render` (reference only) |
|---|------------|---------|--------------------------------|-----------------------------------------------|
| **1** | Draw recorded but **no pixels** pass clip/depth → MRTs stay clear | **Open** | `GBufferPass` sets full RT viewport + depth test/write/Less; matrices from `PerView` / `PerObject` — logic is present; **cannot** confirm raster coverage from repo alone. | Same class of issue possible there too; their lighting read **`u_Position`** so a **missing position write** would also look “empty” independent of depth. |
| **2** | G-buffer writes **A**, deferred samples **B** (different images) | **Unlikely** | `RenderWorld::PrepareUploadPhaseResources` publishes `GBufferRenderTarget` and `MainHdrRenderTarget` from the same `RenderResources::EnsureAllocated` pair; depth is **shared** `MainDepth`. Deferred binds **`gbuffer->GetColorTexture(i)`** / **`gbuffer->GetDepthTexture()`**. | Old graph **imports** named textures (`FrameGraphRecipeBuilders.cpp`); same “one logical depth” idea, different wiring. |
| **3** | **Stale/wrong descriptors** for deferred pass set | **Mitigated** | Pass `dataKey` XORs dirLight + **all three color textures + depth**; `UpdateTexture(0..3)` on change. | Descriptor systems differ; stale-key class of bug was specific to our caching. |
| **4** | **Color** MRTs: attachment write → shader sample **undersynced** | **Unlikely** | Barriers: G-buffer pass requests `RenderTarget` for colors; deferred pass requests `ShaderResource` for same textures before `Execute` (`FrameGraph.cpp` flushes between passes). `ShaderResource` maps to FS-related stages + `SHADER_READ`. | Their `lighting.frag` samples multiple **2D** G-buffer textures with **`v_TexCoord`** — same need for **SRV layout + read sync**; different file layout. |
| **5** | Fullscreen **UV** maps whole screen wrong | **Unlikely** | `fullscreen_triangle.vert` emits `vUv ∈ [0,2]²`; `deferred_lighting_analytic.frag` uses `uv = vUv * 0.5` → **[0,1]²** covering the draw. `DeferredLightingPass` sets HDR viewport to HDR RT size (same allocation dims as G-buffer). | Old **`lighting.vert`** uses mesh **a_TexCoord** already in **[0,1]** — different mesh, same intent. |
| **6** | Running **wrong SPIR-V** / wrong debug defaults | **Out of scope** | (Per user: do **not** evaluate stale-binary / dual-asset hypotheses here.) | — |
| **7** | Deferred **never draws** (early return) vs G-buffer | **Unlikely** | `DeferredLightingPass::Execute` returns only on null cmd/RT/UBO/null descriptor sets; `[DeferredDebug]` logs would fire; user saw fullscreen-style symptoms. | N/A (different pass system). |
| **8** | RenderDoc **mesh viewer** ≠ **FS wrote MRT** | **Diagnostic** | IA/VS can be valid while **FS** doesn’t run or **depth test** rejects before FS — RD still lists draws. | Same limitation for any path. |

**Next single step to shrink #1 vs #4:** In RenderDoc, pick a **pixel over the cube** in the **G-buffer pass** → **Pipeline state** + **pixel history** (did FS run? depth test pass?). If FS never runs, focus **clip/projection/depth**; if FS runs but deferred is still clear, focus **sampling/bindings/barriers** (#3–#5).

### Continued code review (upload → draw, dynamic depth, SSBO) — 2026-05-05

| Check | Verdict | Where |
|--------|---------|--------|
| **Upload then graph on one command buffer** | **OK** | `RenderWorld::RunFrame`: `UploadPhase` then `m_graph.Execute` on the same `recordingCommandList` — uploads always precede passes. |
| **PerObject SSBO copy + visibility** | **Unlikely broken** | `RenderResources::RecordStorageBufferUploadFromStaging`: `TransferDst` → `StorageBuffer` via `BarrierBuffer`; `ToVkPipelineStage(StorageBuffer)` includes **vertex shader**; `ToVkAccessFlags` includes **SHADER_READ** — sufficient for VS `readonly buffer` after the copy in the same buffer. |
| **PerView / dir light UBO uploads** | **Same pattern** | `RecordUniformBufferUploadFromStaging` ends in `UniformBuffer` with stages including VS — OK for `gbuffer_static.vert` bindings. |
| **Dynamic depth state across passes** | **OK if ext. dynamic state on** | `CommandListVK::BindPipeline` reapplies `vkCmdSetDepthTestEnable/Write/CompareOp` from `m_depth_*` when `SupportsExtendedDynamicState()` — **DeferredLightingPass** sets depth off, **GBufferPass** sets on before bind next frame — avoids stale “depth off” on the cmd buffer. If ext. dynamic is **off**, depth comes from **static** pipeline create info (gbuffer spec has depth test/write). |
| **GpuProfiler between upload and passes** | **Low risk** | `GpuProfiler::BeginFrame` only resets/begins statistics query — no render pass or layout change. |

**New matrix row (optional):**

| **9** | **Upload/graph ordering bug** (SSBO/UBO not visible before draw) | **Unlikely** | Same-buffer barriers above; single command stream. | Old pipeline also sequences uploads before lighting in its frame loop (different file layout). |

---

## Changelog

- **2026-05-03:** Document created; deferred pass-color + log instrumentation added per table above.
- **2026-05-03:** Depth/stencil descriptor fix: depth-only SRV view + `GetVkImageViewForShaderSamplerBinding()` (see permanent fixes above).
- **2026-05-03:** `DeferredLightingPass`: sample **G-buffer** depth + barriers on that texture (not Main HDR depth).
- **2026-05-04:** `DeferredLightingPass`: descriptor cache key now includes **G-buffer color texture pointers** (fixes stale sampling after RT realloc).
- **2026-05-04:** RenderDoc: flat G-buffer MRT0 matching debug clear → **(A) confirmed**; doc table **“Where the bug lives in code”** added; matrix row **fragments / depth** updated from “open” to **active on raster path**.
- **2026-05-05:** Added **Hypothesis narrowing** section: code-reviewed matrix vs deprecated `eyelib/render` (reference only); documented remaining **open** item (**fragment-level raster/depth**) and **diagnostic** limit of mesh-only RD inspection; stale SPIR-V / dual-asset marked **out of scope** per user.
- **2026-05-05:** Changelog note: permanent item **#5** — `DepthRead` barrier includes **FS + `SHADER_READ`** for sampled depth (`VulkanEnums.h` rendering + `DeviceVKPresentation.cpp`).
- **2026-05-05:** Continued review: documented **upload→execute** ordering, **PerObject SSBO** barriers (VS-visible), **dynamic depth** refresh on `BindPipeline`, marked **#6** / dual-asset as **out of scope** per user; added matrix **#9** (ordering).
- **2026-05-05:** Added **Localization snapshot**: clarifies we are **not** in a fully unknown space — residual risk is **G-buffer raster (A)** vs **deferred read (B)** fork; ~70–85% of pipeline surface reviewed or fixed in code.
