# Rendering module � implementation order (`Render_steps_todo.md`)

**Purpose:** Single **ordered** checklist for what to build next under `EngineSource/eyelib/rendering/`. Drives the **current scope** (closing **M4** and delivering the first real forward raster path, **M5**). Later phases are parked at the bottom as short anchors; they get expanded into full step tables only when they become the current scope.

**Companion documents**

- `new_render_module.md` � target architecture and non?negotiable rules.
- `newrender_roadmap.md` � phases, exit gates, **�1.3 Hardening dimensions** (D1�D10).
- `Render_check.md` � milestone tracker + changelog; update when steps land.
- `EngineSource/eyelib/rendering/README.md` � on?disk charter.

**Scope (locked, current):**

- `EngineSource/eyelib/rendering/` (everything above RHI). Vulkan RHI itself is tracked separately in `documentation/vulkan/steps_todo.md`.
- Target outcome of current scope: **M4 closed** + **forward raster end?to?end** in the sandbox (rotating/test mesh visible, correct barriers at compile time, real uploads, real visibility ? draw list).
- Out of scope for *this file�s active tables*: deferred, shadows, post/temporal, editor?as?feature, RT/GI/neural, `api/rendering`. Listed at the bottom for continuity.

**How to use:** Implement **top to bottom** within each section. Mark `[x]` when a step lands; add a one?line note with date + files touched. When a section is fully `[x]`, **run the hardening stress check for the listed dimensions** and log a short pass/partial/fail note in `Render_check.md` per the template in `newrender_roadmap.md` �1.3.

**Discipline**

1. **Folders:** Place every new or moved file under the **correct subtree** of `EngineSource/eyelib/rendering/` (and related docs under `documentation/Render_Module/`) per the module charter and roadmap�no ad-hoc locations, no mixing unrelated concerns across top-level areas.
2. **Files & separation of concerns:** Each translation unit should own **one coherent role** (e.g. one pass type, one resource family, one subsystem boundary). Do not scatter one concern across many loosely related files, and do not collapse unrelated logic into a single oversized file; split when responsibilities diverge.
3. **Naming:** Use **precise, professional identifiers** (types, files, passes, blackboard slots, constants)�specific enough that intent and ownership are clear from the name alone; avoid vague, overloaded, or �placeholder� shorthand.

**Build & quality discipline:** After changes under `EngineSource/eyelib/rendering/`, always verify with a **Debug build of `EyelibRendering`** and a **sandbox run of `OpenEyeRenderingSandbox`**. Treat each step as **architecturally correct and complete for its slice**, not �patched until it compiles�.

```
cd D:\Project\World3d\OpenEye; cmake --build build --target EyelibRendering
cd D:\Project\World3d\OpenEye; cmake --build build --target OpenEyeRenderingSandbox
```

---

## Tracking matrix (current scope)


| ID    | Section / Milestone                                                  | Status                                              | Hardening checks after closure                   |
| ----- | -------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------ |
| M0    | Layout scaffold                                                      | [x] Done                                            | �                                                |
| M1    | Core orchestration spine                                             | [x] Done                                            | �                                                |
| M2    | Upload / resources / blackboard / sandbox                            | [x] Done                                            | �                                                |
| M3    | Scene hook / writer registry / compile hook                          | [x] Done                                            | �                                                |
| M4.1  | Raster feature bootstrap                                             | [x] Done                                            | �                                                |
| M4.2  | Fullscreen HDR compose (bootstrap)                                   | [x] Done                                            | �                                                |
| **A** | **M4 closeout** (uploads, visibility, compile?time barriers, naming) | [x] Done (A.10 logged)                              | D1, D2, D4, D5, D6 (see `Render_check.md` �A.10) |
| **B** | **M5 forward raster E2E** (first real feature)                       | [x] Done (B.1�B.4, B.6�B.11; B.5 optional deferred) | D1, D3, D7, D9, D10                              |


Baseline already landed (do not re?implement): see `Render_check.md` �Files touched (M2/M3/M4)� + `EngineSource/eyelib/rendering/README.md`. Vulkan RHI baseline is tracked in `documentation/vulkan/steps_todo.md`.

---

## Section A � M4 closeout � Done

**Exit criteria:**

1. `FrameGraph::Compile` produces a **global barrier plan** (not only execute?time per?pass transitions) or, at minimum, centralizes collection and deduplication at compile time.
2. Real uploads flow through `RenderResources` for **PerFrame / PerView** data (no placeholder blob).
3. `VisibilitySystem` produces a **typed** `DrawList` with frustum-culled entries and populated `sortKey`.
4. Resource/pass names are **central constants** (doc �11); no literal strings scattered in features/passes.
5. `Render_check.md` hardening checks for **D1, D2, D4, D5, D6** logged.


| Step | Status | Item                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A.1  | [x]    | **Central names header.** `framegraph/FrameGraphPassNames.h` rewritten with `pass_names::k*` constants (`kClearBlackboardRenderTarget`, `kFullscreenHdrCompose`, `kBlitMainHdrToDefault`, `kDepthPrepass`, `kForwardOpaque`). New `framegraph/FrameGraphResourceNames.h` with `resource_names::kMainHdrRenderTarget` and `writer_names::{kRenderResources, kRasterFeature}`. Passes use constants in `DebugName()`; `RenderWorld` uses `writer_names::kRenderResources` in `TryReserve` + `PublishRenderTarget`; `RasterFeature::FeatureDebugName` uses `writer_names::kRasterFeature`. No literal strings remain. Debug build green. |
| A.2  | [x]    | **Blackboard slot growth.** `BlackboardSlot` now exposes `MainDepth`, `PerFrameUniformBuffer`, `PerViewUniformBuffer`. `FrameGraphBlackboard` gained `PublishTexture`/`TryGetTexture` + `PublishBuffer`/`TryGetBuffer` (single-writer invariant preserved). `RenderResources` owns a standalone `m_mainDepth` texture and the HDR color RT is now built via `CreateRenderTargetWithSharedDepth`; UBO members reserved (nullptr until A.5). `RenderWorld::Initialize` reserves all four slots for `kRenderResources`; `UploadPhase` publishes HDR + depth (+ UBOs when non-null). `EyelibRendering` Debug build green.                 |
| A.3  | [x]    | **Typed `DrawList`.** `DrawLayer` + `DrawItem` (`meshHandle`, `materialId`, `transformIndex`, `sortKey`, `flags`, `layer`); `DrawList::items` + `VisibleCount()`. Ownership: `RenderWorld::m_drawList`, exposed via `RenderContext::drawList`.                                                                                                                                                                                                                                                                                                                                                                                        |
| A.4  | [x]    | `**VisibilitySystem` � CPU frustum cull.** `IRenderSceneView::SceneDrawInstances` + `SceneDrawInstance` (world matrix + local AABB); `math::Frustum::FromMatrix(proj*view)`; emit visible `DrawItem`s, `sortKey` = layer/material/depth packing, `std::sort` ascending. Sandbox: valid Vulkan camera + two instances (one behind camera, culled). `DeletionQueueVK`: AS destroy via `vkGetDeviceProcAddr` (sandbox link).                                                                                                                                                                                                             |
| A.5  | [x]    | **PerFrame / PerView UBO uploads.** `core/FrameViewUniforms.h` (`PerFrameUniformStd140`, `PerViewUniformStd140`, 256-byte GPU buffers). `RenderResources::EnsureUniformBuffers` + `UploadPerFrameUniform` / `UploadPerViewUniform` (shared staging + barriers). `RasterFeature::OnUpload` fills from `FrameContext` + `RenderContext` / camera. `PipelineLayoutKind::ComposeBootstrap` + `CreateComposeBootstrapPipelineLayout`; compose shader uses set0 binding0/1; `FullscreenHdrComposePass` binds Frame descriptor set.                                                                                                          |
| A.6  | [x]    | **Compile?time barrier collection.** `FrameGraph::Compile(device, &blackboard)` walks passes after `OnCompiled`, calls `CollectTextureBarriers`, `DedupeConsecutiveSameTargetState`, then `MergeBarrierPlansAgainstTracker` (cross-pass: skip if `GetTextureTrackedState` / simulated layout already equals `toState`). `IDevice::GetTextureTrackedState` + `DeviceVK` ? `BarrierTrackerVK::GetImageState`. `Execute` replays merged plan. `RenderWorld` passes blackboard into `Compile`.                                                                                                                                            |
| A.7  | [x]    | **Barrier plan validation (debug).** `FrameGraph.cpp` (`#if !NDEBUG`): intra-pass conflict warn; post-merge asserts + `WarnFirstMergedBarrierTextureUnknownLayout`, `WarnInterPassSimulatedLayoutGap` (budget 32). Removed false-positive �raw fully merged away� warn. `GetTextureTrackedState(const ITexture*)`. Aliasing still future work.                                                                                                                                                                                                                                                                                        |
| A.8  | [x]    | **Upload phase hardening.** `RenderWorld`: `PrepareUploadPhaseResources` (`Ensure*` + blackboard, no recording scope) then `RecordFeatureUploads` (RAII `UploadRecordingPhaseScope` + `OnUpload`). `RenderResources` Debug asserts on staging?GPU record paths + class doc; `Shutdown` asserts scope inactive.                                                                                                                                                                                                                                                                                                                        |
| A.9  | [x]    | **Sandbox check.** Debug `OpenEyeRenderingSandbox` (Vulkan validation on): clear + compose + blit; 2026-04-19 smoke � prior `FrameGraph` false warn removed; `CommandListVK::BeginRenderingWithTarget` skips redundant image transitions when tracker already matches. Note in `Render_check.md`.                                                                                                                                                                                                                                                                                                                                     |
| A.10 | [x]    | **Hardening stress check.** D1�D6 spot-check logged in `Render_check.md` �A.10 (2026-04-19); no dimension **fail**; D4 **partial** (`rhi/RHI.h` ? `core/RenderBackend.h`).                                                                                                                                                                                                                                                                                                                                                                                                                                                            |


---

## Section B � M5 forward raster end?to?end

**Goal:** One complete feature path through the graph that takes real scene data to a real image: **visibility ? PerView upload ? depth prepass (optional) ? forward opaque ? tone map (simple) ? blit**. Forward only; Forward+ / deferred are Phase 6.

**Prerequisites:** Section A exit criteria met. `rhi/Vulkan` baseline sufficient for UBO binding + draw indexed + depth testing (already true).

**Batched delivery order (recommended � complete each batch before the next; no shortcuts):**

1. **Batch 1 � B.1 + B.2:** `ShaderPermutationKey` + `ShaderLibrary` filled in `RasterFeature::OnRegister`; warm-up compile every declared permutation before pass `Execute` binds pipelines. **Done when:** declared keys ? compiled cache, **zero** pipeline compile / `GetOrCompile` in `Execute` or pass bodies (**D7**).
2. **Batch 2 � B.3 + B.4 + B.9:** `IGeometryProvider` + `StaticMeshGeometryProvider`; PerObject SSBO on `RenderResources` uploaded from typed `DrawList` in upload phase (A.8 contract); sandbox scene view with camera + draw instances. **Done when:** provider obeys doc �6 (no lighting reads), uploads only in upload phase, sandbox drives the path.
3. **Batch 3 � B.6 + B.7 + B.8:** Replace bootstrap compose with **clear ? forward opaque ? blit** (barriers per pass); minimal PBR asset pair + matching bindings (PerFrame / PerView / PerObject / light); `**GpuProfiler::Scope` on every pass `Execute`** using `pass_names::k*` (**D10**). **Done when:** first real forward frame to HDR + existing blit; scopes on all new/changed passes. **Landed 2026-04-18** (`RasterForwardM5Minimal`, `ForwardOpaquePass`, `DirectionalLightUniforms`, `GpuProfiler::Scope` in `FrameGraph::Execute`, sandbox `SetMeshResolver`).
4. **Batch 4 � B.5 (optional):** Depth prepass pass only after Batch 3 is stable � own depth-only pipeline, barriers, shared `MainDepth`, provider submit. **Do not** half-merge prepass into Batch 3.
5. **Batch 5 � B.10 + B.11:** Hardening (D1, D3, D7, D9, D10) logged in `Render_check.md`; close M5 + roadmap �11 checklist per B.11.

*Individual B.x rows below stay the source of truth; use the batches above as the implementation sequence.*


| Step | Status | Item                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| B.1  | [x]    | **Permutation declaration.** `shaders/ShaderPermutationKey.h` + `shaders/ShaderLibrary.{h,cpp}`; `RasterFeature::OnRegister` registers compose bootstrap + forward opaque (MSAA 1, 1 directional) specs only � **no device** (2026-04-18).                                                                                                                                                                                                                                      |
| B.2  | [x]    | **Pipeline warm?up.** `RasterFeature::OnSchedulePasses` calls `ShaderLibrary::WarmUpPipelines` (idempotent); compose pass binds cached pipeline only � **no** `CreateGraphicsPipelineFromShaderAsset` in pass `Execute` (D7) (2026-04-18).                                                                                                                                                                                                                                      |
| B.3  | [x]    | **IGeometryProvider + StaticMeshGeometryProvider.** `geometry/IGeometryProvider.h`, `IMeshHandleResolver.h`, `StaticMeshGeometryProvider.{h,cpp}` � `Submit(ICommandList*, DrawList, begin, count)` binds VB/IB from `MeshHandle`, `DrawIndexed` only (no lighting reads) (2026-04-18).                                                                                                                                                                                         |
| B.4  | [x]    | **PerObject SSBO.** `core/PerObjectUniforms.h` (`PerObjectGpuStd430` std430 row); `DrawItem::worldMatrix` + visibility copy; `RenderResources::{EnsurePerObjectBuffer,UploadPerObjectBufferFromDrawList,PerObjectStorageBuffer}`; blackboard `PerObjectStorageBuffer`; `ResourceState::StorageBuffer` + barriers (2026-04-18).                                                                                                                                                  |
| B.5  | [ ]    | `**DepthPrepassPass` (optional, recommended).** Graph pass that binds depth?only pipeline + PerView + PerObject and submits via the provider. Writes `MainDepth`.                                                                                                                                                                                                                                                                                                               |
| B.6  | [x]    | **ForwardOpaquePass (real).** `RasterFeature::OnSchedulePasses` is clear ? `ForwardOpaquePass` ? blit. Pass binds Frame + `RasterForwardM5Data` (PerObject SSBO + directional UBO), `pass_names::kForwardOpaque`, barriers like clear pass. `BlackboardSlot::DirectionalLightUniformBuffer` + publish from `RenderWorld`. **Vulkan:** `PipelineLayoutKind::RasterForwardM5Minimal`, `ResourceSetType::RasterForwardM5Data`, `DeviceVK` pipeline/set layout wiring (2026-04-18). |
| B.7  | [x]    | **Minimal PBR shader.** Embedded GLSL in `RasterFeature.cpp` (`CompileFromSource`), Lambert + Blinn-Phong-style spec + ambient; material id tints albedo; one directional light std140 (256 B). Push constant `objectIndex` for SSBO row (2026-04-18).                                                                                                                                                                                                                          |
| B.8  | [x]    | **Graph pass-scope profiling.** `GpuProfiler::Scope` (debug-utils labels via `ICommandList::PushDebugUtilsLabel` / `Pop`); `FrameGraph::Execute` wraps each pass with `DebugName()` (same strings as `pass_names::k`*) (2026-04-18).                                                                                                                                                                                                                                            |
| B.9  | [x]    | **Sandbox scene.** `rendering_sandbox/main.cpp`: unit-cube `Mesh::Create`, `SandboxMeshResolver` (handle **0**), `StaticMeshGeometryProvider` instance (for Batch 3 `Submit`); scene uses mesh **0** + material ids; teardown calls `Mesh::Destroy(device)` (2026-04-18).                                                                                                                                                                                                       |
| B.10 | [x]    | **Hardening stress check.** Run **D1** (graph integrity incl. new passes), **D3** (feature isolation � RasterFeature still owns its state, no cross?feature includes), **D7** (capabilities / permutations � no compile in execute), **D9** (temporal ownership � no stray history allocations from RasterFeature), **D10** (observability � scopes on every pass). Logged in `Render_check.md` (2026-05-02).                                                                   |
| B.11 | [x]    | **Close M5 in `Render_check.md`.** Mark M4 fully done, M5 done, update "Files touched" section. Flip the �11 printable checklist in `newrender_roadmap.md` (P4/P5 -> done). Done (2026-05-02).                                                                                                                                                                                                                                                                                  |


---

### Bx - Attachment ops contract (post-M5 stabilization, pre-P6)

**Why here:** The invisible-cube investigation proved current forward now works, but it also exposed fragility in clear/load semantics with dynamic rendering. This refactor should land **after M5 is stable** and **before deferred/shadow growth** so pass IO semantics are explicit before pass count expands.

**Exit criteria:**

1. Passes declare attachment load/store intent explicitly (color/depth + clear values where relevant).
2. CommandListVK begins dynamic rendering from pass-declared ops (not implicit LOAD + separate clear dependency).
3. First-writer clear path for forward raster is local to forward pass begin (LOAD_OP_CLEAR) and deterministic.
4. Compile-time validation warns on conflicting attachment ops for the same resource.
5. Render_check.md logs D1/D2/D6 hardening after migration.


| Step | Status | Item                                                                                                                                                                                                                                            |
| ---- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Bx.1 | [x]    | **Pass-level contract type.** Added `framegraph/FrameGraphAttachmentOps.h` (attachment load/store ops + clear values, backend-agnostic). Done (2026-05-02).                                                                                     |
| Bx.2 | [x]    | **Pass API hookup.** `IFrameGraphPass::DescribeRenderAttachmentOps(...)` added as optional backend-agnostic contract for render-target begin passes. Done (2026-05-02).                                                                         |
| Bx.3 | [x]    | **Vulkan mapping.** `ICommandList::SetRenderTarget(rt, ops)` and `CommandListVK::BeginRenderingWithTarget(..., ops)` map load/store/clear to `VkRenderingAttachmentInfo`; default path still preserved when no ops provided. Done (2026-05-02). |
| Bx.4 | [x]    | **Forward path migration.** `ForwardOpaquePass` now begins HDR render target with explicit color/depth `LOAD_OP_CLEAR`; default raster schedule no longer injects standalone clear pass before forward. Done (2026-05-02).                      |
| Bx.5 | [x]    | **Compile-time checks.** `FrameGraph::Compile` now validates conflicting declared attachment ops for the same render-target textures and emits bounded debug warnings. Done (2026-05-02).                                                       |
| Bx.6 | [x]    | **Sandbox + docs closeout.** Debug build + sandbox smoke performed; docs updated with Bx implementation + validation notes. Done (2026-05-02).                                                                                                  |


---

## Section P6 - Immediate implementation sequence (active next)

**Goal:** implement full deferred PBR plus full material system as a first-class next-gen feature path in `eyelib/rendering/`, following frame-graph ownership, feature isolation, and upload-phase discipline from `new_render_module.md` and `newrender_roadmap.md`.

**Architecture guardrails (mandatory during all P6 steps):**

- All GPU writes remain graph-pass owned (D1); no ad-hoc tail recording.
- Feature communication is blackboard-only (D2/D3); no cross-feature direct calls.
- Material/lights data upload happens in upload phase only (D5), not lazily in pass execute.
- No `GetOrCompile` in execute (D7); permutations and capabilities are decided in register/warm-up.

| Step | Status | Implementation order (whole system) |
| ---- | ------ | ---- |
| P6.1 | [ ] | **Freeze deferred contracts.** Finalize and document set/binding contracts for `RasterGBuffer` and `RasterDeferredLighting` (set ownership, resource types, push constants, attachment formats). This is the source of truth before coding additional material features. |
| P6.2 | [x] | **Define canonical material model.** Landed: `materials/MaterialSchema.h` with typed CPU `MaterialRecord` + std430 `MaterialGpuStd430` packing/helpers (2026-05-05). |
| P6.3 | [x] | **Create material runtime ownership path.** Landed: `materials/MaterialTable.{h,cpp}` runtime ID-addressed table (slot 0 default, row generation for GPU upload) (2026-05-05). |
| P6.4 | [x] | **Upload path integration.** Landed: `RenderResources` material-table SSBO allocation/upload + blackboard slot publication (`MaterialTableStorageBuffer`); upload remains upload-phase only (2026-05-05). |
| P6.5 | [x] | **GBuffer system completion.** Landed: `RasterGBufferData` now binds PerObject + MaterialTable SSBO; `gbuffer_static` writes deterministic material-driven GBuffer data (albedo, normal, metallic, roughness, AO) plus emissive-strength/flags channels (2026-05-05). |
| P6.6 | [x] | **Deferred lighting PBR shading.** Landed: `deferred_lighting_analytic.frag` upgraded from phong-like shading to GGX-based direct-light PBR (NDF + geometry + fresnel + energy-conserving diffuse/spec split) consuming P6 material channels (2026-05-05). |
| P6.7 | [x] | **IBL and environment contribution.** Contract + split-sum shader + blackboard; **ownership:** `lighting/ibl/IblDefaultProbes` + `IblCpuDefaults`, `RasterFeature` reserves/publishes `Ibl*` slots under `writer_names::kLightingIbl`; `RenderResources` has no IBL; `DeferredLightingPass` barriers + descriptor cache keys; GPU env bake remains `IBL` class. BRDF LUT startup policy: asset-backed raw LUT first (`assets/textures/.../brdf_lut_rg16f_256x256.bin`), then GPU one-shot fallback via `IBL::Initialize`, bounded CPU fallback (256², 64 samples) only as last resort; timing/source logs added (2026-05-05). |
| P6.8 | [ ] | **Material texture/fallback parity.** Validate all supported material combinations: scalar-only, partial-texture, full-texture, missing normal/metallic-roughness/emissive paths. Ensure no undefined reads and no contract mismatch between CPU packing and shader usage. _In progress (2026-05-06): RasterGBuffer now includes bindless set for material texture sampling; `gbuffer_static` consumes material texture handles with explicit fallback guards; sandbox direct-GLB path now populates `MaterialTable` texture handles from Assimp materials (file + embedded compressed textures)._ |
| P6.9 | [ ] | **Mode-switch and graph correctness.** Verify forward<->deferred runtime switching keeps descriptor state, blackboard resources, and barriers correct (no stale set reuse, no leftover resources from previous mode). |
| P6.10 | [ ] | **Performance and stability pass.** Remove per-draw material churn where avoidable, verify descriptor reuse strategy, and confirm upload costs are bounded. Keep any optimization within architecture rules (no bypass of graph/upload contracts). |
| P6.11 | [ ] | **Validation matrix in sandbox.** Run scripted checks: multi-material scene, texture/no-texture mix, normal map stress, metallic/roughness extremes, emissive-only, camera motion, and hot mode switching; log pass/fail notes. |
| P6.12 | [ ] | **Hardening closeout.** Record D1/D2/D3/D5/D6/D7/D10 checks in `render_check.md`, then mark P6 complete and move Section C pointers to next active scope. |

---

## Section C � Parking lot (next?scope anchors, **not** active)

Keep as pointers only; expand into full tables when promoted to current scope.

- **P6:** moved to active section above (`Section P6 - Immediate implementation sequence`).
- **P7 � Shadows + light grids.** `lighting/shadows/` (CSM strategy); Forward+ clustered light grid.
- **P8 � Post + temporal in the graph.** Tone map, bloom, exposure; `temporal/` with history ownership and camera?cut invalidation.
- **P9 � Editor as a feature.** `features/editor/` registers like any other feature; zero cost when not registered.
- **P10 � Optional tracks.** RT (`features/raytracing/`), GI (`lighting/gi/`), atmosphere, upscaling/denoising, neural/splats.
- **P11 � Public API + migration.** `api/rendering/` PIMPL; migrate sandbox ? editor/player; remove `eyelib/render` when no target links it.

---

## Cross?cutting rules (repeat to resist drift)

- If it writes a texture or buffer, it is a **graph pass** (doc �1, D1).
- Features talk **only** via the blackboard; disabled features register **zero** passes (doc �2/�3, D2/D3).
- `scene ? core ? framegraph ? scheduler ? features ? � ? rhi` � no reverse includes (D4).
- Capabilities captured at `OnRegister`; **no** `GetOrCompile` in `Execute` (doc �5/�10, D7).
- No `ICommandList`* stored across frames (doc �9, D8).
- All resource/pass names come from central constants (doc �11).
- GPU scopes on every pass from the first real feature on (doc �13, D10).

---

*Update this file whenever a step lands (status + one?line note). Promote Section C items into full tables only when they become the current scope.*