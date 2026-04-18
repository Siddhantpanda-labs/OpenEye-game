# Rendering module — implementation order (`Render_steps_todo.md`)

**Purpose:** Single **ordered** checklist for what to build next under `EngineSource/eyelib/rendering/`. Drives the **current scope** (closing **M4** and delivering the first real forward raster path, **M5**). Later phases are parked at the bottom as short anchors; they get expanded into full step tables only when they become the current scope.

**Companion documents**

- `new_render_module.md` — target architecture and non‑negotiable rules.
- `newrender_roadmap.md` — phases, exit gates, **§1.3 Hardening dimensions** (D1–D10).
- `Render_check.md` — milestone tracker + changelog; update when steps land.
- `EngineSource/eyelib/rendering/README.md` — on‑disk charter.

**Scope (locked, current):**

- `EngineSource/eyelib/rendering/` (everything above RHI). Vulkan RHI itself is tracked separately in `documentation/vulkan/steps_todo.md`.
- Target outcome of current scope: **M4 closed** + **forward raster end‑to‑end** in the sandbox (rotating/test mesh visible, correct barriers at compile time, real uploads, real visibility → draw list).
- Out of scope for *this file’s active tables*: deferred, shadows, post/temporal, editor‑as‑feature, RT/GI/neural, `api/rendering`. Listed at the bottom for continuity.

**How to use:** Implement **top to bottom** within each section. Mark `[x]` when a step lands; add a one‑line note with date + files touched. When a section is fully `[x]`, **run the hardening stress check for the listed dimensions** and log a short pass/partial/fail note in `Render_check.md` per the template in `newrender_roadmap.md` §1.3.

**Discipline:** After changes under `EngineSource/eyelib/rendering/`, always verify with a **Debug build of `EyelibRendering`** and a **sandbox run of `OpenEyeRenderingSandbox`**. Treat each step as **architecturally correct and complete for its slice**, not “patched until it compiles”.

```
cd D:\Project\World3d\OpenEye; cmake --build build --target EyelibRendering
cd D:\Project\World3d\OpenEye; cmake --build build --target OpenEyeRenderingSandbox
```

---

## Tracking matrix (current scope)


| ID    | Section / Milestone                                                  | Status              | Hardening checks after closure |
| ----- | -------------------------------------------------------------------- | ------------------- | ------------------------------ |
| M0    | Layout scaffold                                                      | [x] Done            | —                              |
| M1    | Core orchestration spine                                             | [x] Done            | —                              |
| M2    | Upload / resources / blackboard / sandbox                            | [x] Done            | —                              |
| M3    | Scene hook / writer registry / compile hook                          | [x] Done            | —                              |
| M4.1  | Raster feature bootstrap                                             | [x] Done            | —                              |
| M4.2  | Fullscreen HDR compose (bootstrap)                                   | [x] Done            | —                              |
| **A** | **M4 closeout** (uploads, visibility, compile‑time barriers, naming) | [x] Done (A.10 logged) | D1, D2, D4, D5, D6 (see `Render_check.md` §A.10) |
| **B** | **M5 forward raster E2E** (first real feature)                       | [ ] Not started     | D1, D3, D7, D9, D10            |


Baseline already landed (do not re‑implement): see `Render_check.md` “Files touched (M2/M3/M4)” + `EngineSource/eyelib/rendering/README.md`. Vulkan RHI baseline is tracked in `documentation/vulkan/steps_todo.md`.

---

## Section A — M4 closeout — Done

**Exit criteria:**

1. `FrameGraph::Compile` produces a **global barrier plan** (not only execute‑time per‑pass transitions) or, at minimum, centralizes collection and deduplication at compile time.
2. Real uploads flow through `RenderResources` for **PerFrame / PerView** data (no placeholder blob).
3. `VisibilitySystem` produces a **typed** `DrawList` with frustum-culled entries and populated `sortKey`.
4. Resource/pass names are **central constants** (doc §11); no literal strings scattered in features/passes.
5. `Render_check.md` hardening checks for **D1, D2, D4, D5, D6** logged.


| Step | Status | Item                                                                                                                                                                                                                                                                                                                                                                                                          |
| ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A.1  | [x]    | **Central names header.** `framegraph/FrameGraphPassNames.h` rewritten with `pass_names::k*` constants (`kClearBlackboardRenderTarget`, `kFullscreenHdrCompose`, `kBlitMainHdrToDefault`, `kDepthPrepass`, `kForwardOpaque`). New `framegraph/FrameGraphResourceNames.h` with `resource_names::kMainHdrRenderTarget` and `writer_names::{kRenderResources, kRasterFeature}`. Passes use constants in `DebugName()`; `RenderWorld` uses `writer_names::kRenderResources` in `TryReserve` + `PublishRenderTarget`; `RasterFeature::FeatureDebugName` uses `writer_names::kRasterFeature`. No literal strings remain. Debug build green. |
| A.2  | [x]    | **Blackboard slot growth.** `BlackboardSlot` now exposes `MainDepth`, `PerFrameUniformBuffer`, `PerViewUniformBuffer`. `FrameGraphBlackboard` gained `PublishTexture`/`TryGetTexture` + `PublishBuffer`/`TryGetBuffer` (single-writer invariant preserved). `RenderResources` owns a standalone `m_mainDepth` texture and the HDR color RT is now built via `CreateRenderTargetWithSharedDepth`; UBO members reserved (nullptr until A.5). `RenderWorld::Initialize` reserves all four slots for `kRenderResources`; `UploadPhase` publishes HDR + depth (+ UBOs when non-null). `EyelibRendering` Debug build green. |
| A.3  | [x]    | **Typed `DrawList`.** `DrawLayer` + `DrawItem` (`meshHandle`, `materialId`, `transformIndex`, `sortKey`, `flags`, `layer`); `DrawList::items` + `VisibleCount()`. Ownership: `RenderWorld::m_drawList`, exposed via `RenderContext::drawList`.                                                                                                                                                                  |
| A.4  | [x]    | **`VisibilitySystem` — CPU frustum cull.** `IRenderSceneView::SceneDrawInstances` + `SceneDrawInstance` (world matrix + local AABB); `math::Frustum::FromMatrix(proj*view)`; emit visible `DrawItem`s, `sortKey` = layer/material/depth packing, `std::sort` ascending. Sandbox: valid Vulkan camera + two instances (one behind camera, culled). `DeletionQueueVK`: AS destroy via `vkGetDeviceProcAddr` (sandbox link). |
| A.5  | [x]    | **PerFrame / PerView UBO uploads.** `core/FrameViewUniforms.h` (`PerFrameUniformStd140`, `PerViewUniformStd140`, 256-byte GPU buffers). `RenderResources::EnsureUniformBuffers` + `UploadPerFrameUniform` / `UploadPerViewUniform` (shared staging + barriers). `RasterFeature::OnUpload` fills from `FrameContext` + `RenderContext` / camera. `PipelineLayoutKind::ComposeBootstrap` + `CreateComposeBootstrapPipelineLayout`; compose shader uses set0 binding0/1; `FullscreenHdrComposePass` binds Frame descriptor set. |
| A.6  | [x]    | **Compile‑time barrier collection.** `FrameGraph::Compile(device, &blackboard)` walks passes after `OnCompiled`, calls `CollectTextureBarriers`, `DedupeConsecutiveSameTargetState`, then `MergeBarrierPlansAgainstTracker` (cross-pass: skip if `GetTextureTrackedState` / simulated layout already equals `toState`). `IDevice::GetTextureTrackedState` + `DeviceVK` → `BarrierTrackerVK::GetImageState`. `Execute` replays merged plan. `RenderWorld` passes blackboard into `Compile`. |
| A.7  | [x]    | **Barrier plan validation (debug).** `FrameGraph.cpp` (`#if !NDEBUG`): intra-pass conflict warn; post-merge asserts + `WarnFirstMergedBarrierTextureUnknownLayout`, `WarnInterPassSimulatedLayoutGap` (budget 32). Removed false-positive “raw fully merged away” warn. `GetTextureTrackedState(const ITexture*)`. Aliasing still future work. |
| A.8  | [x]    | **Upload phase hardening.** `RenderWorld`: `PrepareUploadPhaseResources` (`Ensure*` + blackboard, no recording scope) then `RecordFeatureUploads` (RAII `UploadRecordingPhaseScope` + `OnUpload`). `RenderResources` Debug asserts on staging→GPU record paths + class doc; `Shutdown` asserts scope inactive. |
| A.9  | [x]    | **Sandbox check.** Debug `OpenEyeRenderingSandbox` (Vulkan validation on): clear + compose + blit; 2026-04-19 smoke — prior `FrameGraph` false warn removed; `CommandListVK::BeginRenderingWithTarget` skips redundant image transitions when tracker already matches. Note in `Render_check.md`. |
| A.10 | [x]    | **Hardening stress check.** D1–D6 spot-check logged in `Render_check.md` §A.10 (2026-04-19); no dimension **fail**; D4 **partial** (`rhi/RHI.h` → `core/RenderBackend.h`).                                                                                                                                                                                                                         |


---

## Section B — M5 forward raster end‑to‑end

**Goal:** One complete feature path through the graph that takes real scene data to a real image: **visibility → PerView upload → depth prepass (optional) → forward opaque → tone map (simple) → blit**. Forward only; Forward+ / deferred are Phase 6.

**Prerequisites:** Section A exit criteria met. `rhi/Vulkan` baseline sufficient for UBO binding + draw indexed + depth testing (already true).


| Step | Status | Item                                                                                                                                                                                                                                                                                                                                                                                          |
| ---- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| B.1  | [ ]    | **Permutation declaration.** Add `shaders/ShaderPermutationKey` + a small `ShaderLibrary` that a feature fills in `OnRegister`. `RasterFeature::OnRegister` declares its permutations (forward opaque; MSAA off; 1 directional light). **No `GetOrCompile` in `Execute`** (D7).                                                                                                               |
| B.2  | [ ]    | **Pipeline warm‑up.** On first `OnSchedulePasses` (or background before first frame), compile all declared permutations via existing `IDevice::CreateGraphicsPipelineFromShaderAsset`. Cache by key; passes only **bind** cached pipelines.                                                                                                                                                   |
| B.3  | [ ]    | `**IGeometryProvider`.** Add `geometry/IGeometryProvider.h` with `Submit(cmd, drawList, range)` + a default `StaticMeshGeometryProvider` that resolves mesh/material handles from existing `resources/` and emits `DrawIndexed`. Providers never read lighting (doc §6).                                                                                                                      |
| B.4  | [ ]    | **PerObject buffer.** `RenderResources` grows a **PerObject SSBO** (transforms + material ids) uploaded in the upload phase from the typed `DrawList`. Bound once at pass start; draws use `gl_BaseInstance` / push constant to index it.                                                                                                                                                     |
| B.5  | [ ]    | `**DepthPrepassPass` (optional, recommended).** Graph pass that binds depth‑only pipeline + PerView + PerObject and submits via the provider. Writes `MainDepth`.                                                                                                                                                                                                                             |
| B.6  | [ ]    | `**ForwardOpaquePass` (real).** Replace the compose bootstrap path in `RasterFeature::OnSchedulePasses` with: clear HDR + depth → (optional) depth prepass → forward opaque → blit. Bind PerFrame + PerView + PerObject + (single directional light) UBOs. Sample no shadow map yet.                                                                                                          |
| B.7  | [ ]    | **Minimal PBR shader.** Ship one `assets/shaders/` pair (vert/frag) under the new module’s asset policy decision (see `newrender_roadmap.md` §7). GGX + Lambert + one directional light. Push constant for material overrides allowed for now.                                                                                                                                                |
| B.8  | [ ]    | **Graph pass‑scope profiling.** Wrap each pass `Execute` with a `GpuProfiler::Scope` using the `FrameGraphPassNames.h` constants; `RenderStats` already captures this. (D10.)                                                                                                                                                                                                                 |
| B.9  | [ ]    | **Sandbox scene.** `rendering_sandbox/main.cpp` adds a minimal scene view that returns a camera + a small list of draw items (one or two static meshes + a material). No legacy `eyelib/render` dependency.                                                                                                                                                                                   |
| B.10 | [ ]    | **Hardening stress check.** Run **D1** (graph integrity incl. new passes), **D3** (feature isolation — RasterFeature still owns its state, no cross‑feature includes), **D7** (capabilities / permutations — no compile in execute), **D9** (temporal ownership — no stray history allocations from RasterFeature), **D10** (observability — scopes on every pass). Log in `Render_check.md`. |
| B.11 | [ ]    | **Close M5 in `Render_check.md`.** Mark M4 fully done, M5 done, update “Files touched” section. Flip the §11 printable checklist in `newrender_roadmap.md` (P4/P5 → done).                                                                                                                                                                                                                    |


---

## Section C — Parking lot (next‑scope anchors, **not** active)

Keep as pointers only; expand into full tables when promoted to current scope.

- **P6 — Deferred + mode switching inside `RasterFeature`.** GBuffer + deferred lighting pass; feature config picks forward vs deferred.
- **P7 — Shadows + light grids.** `lighting/shadows/` (CSM strategy); Forward+ clustered light grid.
- **P8 — Post + temporal in the graph.** Tone map, bloom, exposure; `temporal/` with history ownership and camera‑cut invalidation.
- **P9 — Editor as a feature.** `features/editor/` registers like any other feature; zero cost when not registered.
- **P10 — Optional tracks.** RT (`features/raytracing/`), GI (`lighting/gi/`), atmosphere, upscaling/denoising, neural/splats.
- **P11 — Public API + migration.** `api/rendering/` PIMPL; migrate sandbox → editor/player; remove `eyelib/render` when no target links it.

---

## Cross‑cutting rules (repeat to resist drift)

- If it writes a texture or buffer, it is a **graph pass** (doc §1, D1).
- Features talk **only** via the blackboard; disabled features register **zero** passes (doc §2/§3, D2/D3).
- `scene → core → framegraph → scheduler → features → … → rhi` — no reverse includes (D4).
- Capabilities captured at `OnRegister`; **no** `GetOrCompile` in `Execute` (doc §5/§10, D7).
- No `ICommandList`* stored across frames (doc §9, D8).
- All resource/pass names come from central constants (doc §11).
- GPU scopes on every pass from the first real feature on (doc §13, D10).

---

*Update this file whenever a step lands (status + one‑line note). Promote Section C items into full tables only when they become the current scope.*