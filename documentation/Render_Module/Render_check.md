# Render module — implementation track & checks

**Spec:** `new_render_module.md`  
**Code:** `EngineSource/eyelib/rendering/`  
**Build check:** `EyelibRendering` (Debug), `OpenEyeRenderingSandbox` (Debug)

---

## Discipline (layout, ownership, naming)

1. **Folders:** Keep every file in the **right folder** under `EngineSource/eyelib/rendering/` (and this doc tree under `documentation/Render_Module/`) per the module charter and roadmap—no ad-hoc locations, no mixing unrelated concerns across top-level areas.
2. **Files & separation of concerns:** Each file should own **one coherent role** (e.g. one pass, one resource family, one subsystem boundary). Do not scatter one concern across many loosely related files, and do not merge unrelated logic into a single oversized file; split when responsibilities diverge.
3. **Naming:** Use **precise, professional identifiers** (types, files, passes, blackboard slots)—specific enough that intent and ownership are obvious from the name alone; avoid vague or “placeholder” shorthand.

---

## Principles (ongoing)


| Check                                   | Status                                                                     |
| --------------------------------------- | -------------------------------------------------------------------------- |
| GPU work only through frame graph       | In progress                                                                |
| Inter-feature comms via blackboard only | In progress — registration-time writers + per-frame publish checks (debug) |
| Dependency direction                    | Enforced by includes; review each PR                                       |
| Capabilities queried once at init       | Done                                                                       |
| No `ICommandList`* stored across frames | Passes do not retain `cmd`                                                 |
| Feature subscription                       | Done — `TrySubscribeFeature` (late = immediate `OnRegister`), `TryUnsubscribeFeature` anytime (runtime = `OnUnregisterBlackboardWriters` + `OnShutdown` + `TryRelease` slots), duplicate-safe, `FeatureDebugName` |
| Blackboard survives upload → schedule      | Done — scheduler clears graph only, not blackboard (core publishes kept) |


---

## Milestones

### M0 — Layout scaffold — Done

### M1 — Core orchestration spine — Done

**API note:** `IRenderFeature::OnSchedulePasses(..., const RenderContext& render)` for device access during scheduling.

### M2 — Upload, resources, blackboard, passes, sandbox — Done

### M3 — Scene hook, blackboard registry, compile hooks — Done


| Item                                                                           | Status |
| ------------------------------------------------------------------------------ | ------ |
| `RenderContext::sceneView` + `RenderWorld::SetSceneView`                       | Done   |
| `IRenderSceneView::TryGetCamera` / `RenderSceneView`                           | Done   |
| `BlackboardWriterRegistry` + `OnRegisterBlackboardWriters`                     | Done   |
| Debug: publish must match reserved writer when registry is bound               | Done   |
| `IFrameGraphPass::OnCompiled` + `FrameGraph::Compile(device, blackboard)` pass-index + barrier plan  | Done   |
| `RenderWorld::Initialize` reserves `MainHdrRenderTarget` for `RenderResources` | Done   |


### M4.1 — Raster feature bootstrap — Done

| Item | Status |
|------|--------|
| `RasterFeature` as `IRenderFeature` (HDR clear + blit + upload scratch demo) | Done |
| Sandbox uses `RasterFeature` only (no inline feature class) | Done |

**Files:** `features/raster/RasterFeature.h`, `RasterFeature.cpp`, `app/rendering_sandbox/main.cpp`

### M4.2 — Fullscreen HDR compose (deferred bootstrap) — Done

| Item | Status |
|------|--------|
| `FullscreenHdrComposePass` (barriers + `DrawFullscreenQuad`, push tint) | Done |
| `RasterFeature` pipelines via `shaders/ShaderLibrary` — `OnRegister` declares permutations, `OnSchedulePasses` warms all (compose + forward baseline); compose still uses embedded GLSL overrides | Done |
| `OnShutdown` destroys cached pipelines (`ShaderLibrary::DestroyPipelines`) | Done |
| **Batch 2 (B.3–B.4, B.9):** `geometry/IGeometryProvider` + `StaticMeshGeometryProvider`; PerObject SSBO + blackboard slot; sandbox unit cube + `IMeshHandleResolver` | Done |
| **Batch 3 (B.6–B.8):** Default graph **clear → forward opaque → blit** (`ForwardOpaquePass`, `PipelineLayoutKind::RasterForwardM5Minimal`, directional-light UBO, embedded M5 shaders); `GpuProfiler::Scope` + `FrameGraph::Execute` pass labels; sandbox calls `RasterFeature::SetMeshResolver` | Done (2026-04-18) |

### M4 — Uploads, barriers, visibility → draw list — Section **A** closeout done (A.10 hardening logged); full M4 / M5 forward path still tracked below


| Item                                                                                      | Status                                                                 |
| ----------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Staging upload buffer capacity + `EnsureStagingUploadBuffer` / `Shutdown` ordering       | Done                                                                   |
| Real staging uploads in `OnUpload` (buffers, material tables)                             | Partial — PerFrame/PerView UBO path live; `RecordGpuUploadScratchFromStaging` retained for misc; material tables TBD |
| `FrameGraph::Compile` per-pass barriers + dedupe + cross-pass noop merge vs `BarrierTrackerVK` | Done — `MergeBarrierPlansAgainstTracker`, `IDevice::GetTextureTrackedState` |
| `FrameGraph::Execute` applies compiled barrier plan → transition → flush → pass body     | Done                                                                   |
| `VisibilitySystem::BuildCpuDrawList` + `RenderContext::drawList` + `UploadPhase` wiring   | Done — `SceneDrawInstance` stream, frustum cull, `sortKey` + sort (GPU cull still future) |
| Pass texture barrier hooks (`IFrameGraphPass::CollectTextureBarriers`)                  | Done — clear (RT/depth) + blit (HDR color → `ShaderResource` before `BlitToDefaultFramebuffer`) |
| **A.1** Central names header (`pass_names::k*`, `writer_names::k*`, `resource_names::k*`) | Done — no literal pass/writer strings remain |
| **A.2** Blackboard slot growth (`MainDepth`, `PerFrame/PerView` UBOs) + shared-depth split | Done — blackboard publishes textures/buffers, HDR uses `CreateRenderTargetWithSharedDepth`, UBO slots reserved (null until A.5) |
| **A.3** Typed `DrawList` (`DrawLayer`, `DrawItem`, `items`, `VisibleCount`)                | Done |
| **A.4** CPU frustum visibility + `sortKey` in `VisibilitySystem`                           | Done — scene instances + `math::Frustum`; sandbox exercises cull |
| **A.5** PerFrame / PerView UBOs + upload path + compose bind                               | Done — `FrameViewUniforms`, `ComposeBootstrap` layout, descriptor bind in compose pass |
| **A.6** Compile-time barrier plans + global merge vs tracker                          | Done — `m_barrierPlans`, per-pass dedupe, `MergeBarrierPlansAgainstTracker`, `GetTextureTrackedState` |
| **A.7** Debug barrier validation (intra + inter-pass warns, merged asserts)          | Done — late Undefined / sim-gap `WarnF`; false-positive “raw merged away” removed |
| **A.8** Upload phase guard (`RenderResources` GPU uploads only in `OnUpload`)        | Done — prepare (`Ensure*` + blackboard) vs record (`OnUpload` + scope); Debug asserts |
| **A.9** Sandbox smoke (`OpenEyeRenderingSandbox` Debug, validation on)               | Done — 2026-04-19: clear + compose + blit; no spurious `FrameGraph` / redundant-tracker noise after fixes below |


**A.9 smoke (2026-04-19):** `FullscreenHdrComposePass` no longer duplicates `CollectTextureBarriers` (clear pass owns RT/depth layouts). `CommandListVK::BeginRenderingWithTarget` only calls `TransitionTextureToState` when `GetTextureTrackedState` differs (avoids duplicate transitions after graph flush). `WarnInterPassRawBarriersFullyMergedAway` removed from `FrameGraph::Compile`.

### A.10 — Section A hardening (§1.3 template, 2026-04-19)

Scope: milestone **M4 closeout** after A.1–A.9. **No fail** — safe to start Section B planning.

- **Dimension:** D1 (Frame graph integrity)  
  **Inspected:** `core/RenderWorld.cpp` (`RunFrame`: `UploadPhase` → `Schedule` → `Compile` → `Execute` only; no GPU tail after `m_graph.Execute`), `framegraph/FrameGraph.cpp` (`Execute` → pass `Execute` only), passes under `framegraph/passes/*` (no `ICommandList` stored on pass objects).  
  **Result:** **pass**  
  **Notes:** HDR path GPU work for bootstrap is clear/compose/blit passes; upload uses same cmd list before graph (by design).

- **Dimension:** D2 (Blackboard hygiene)  
  **Inspected:** `FrameGraphBlackboard.h` (typed slots + writer validation), `RenderWorld::Initialize` / `UploadPhase` publishes with `writer_names::kRenderResources`, `RasterFeature` uses `pass_names::k*` / `BlackboardSlot` (no literal slot strings in hot paths).  
  **Result:** **pass**  
  **Notes:** Single-writer registry for four slots; features do not publish core-owned HDR in current bootstrap.

- **Dimension:** D4 (Dependency direction)  
  **Inspected:** `rg` include sweep — `framegraph/**` does not include `scheduler/` or `features/`; `scene/**` does not include `framegraph`/`scheduler`/`features`. `core/RenderWorld.*` includes scheduler (orchestration owns scheduler — expected).  
  **Result:** **partial**  
  **Notes:** `rhi/RHI.h` includes `core/RenderBackend.h` (thin policy header); document or trim later if strict “rhi above nothing” is required.

- **Dimension:** D5 (Upload phase reality)  
  **Inspected:** `rg` for `UploadPerFrame|UploadPerView|RecordGpuUploadScratch` under `eyelib/rendering` — only `RasterFeature::OnUpload` + `RenderResources` impl. `RenderResources` Debug asserts + `PrepareUploadPhaseResources` / `RecordFeatureUploads` / RAII scope in `RenderWorld.cpp`.  
  **Result:** **pass**  
  **Notes:** No pass `Execute` calls `RenderResources` upload APIs; `RasterFeature::OnUpload` redundant `EnsureUniformBuffers` is harmless (idempotent).

- **Dimension:** D6 (Barriers & scheduling)  
  **Inspected:** `FrameGraph::Compile` (`CollectTextureBarriers`, dedupe, `MergeBarrierPlansAgainstTracker`, A.7 validators), `FrameGraph::Execute` (replays `m_barrierPlans`); aliasing / async-compute ordering not in scope yet.  
  **Result:** **partial**  
  **Notes:** Compile-time plan + tracker merge is in place; resource aliasing and async queue scheduling remain future graph work.

---

## Files touched (M4)

- `framegraph/FrameGraphBarrierList.h` (new)
- `framegraph/FrameGraphPass.h`, `FrameGraph.h`, `FrameGraph.cpp`
- `framegraph/passes/ClearBlackboardRenderTargetPass.h`, `ClearBlackboardRenderTargetPass.cpp`
- `framegraph/passes/BlitMainHdrToDefaultPass.h`, `BlitMainHdrToDefaultPass.cpp`
- `framegraph/passes/FullscreenHdrComposePass.h`, `FullscreenHdrComposePass.cpp`
- `scene/DrawList.h`
- `visibility/VisibilitySystem.h`, `VisibilitySystem.cpp`
- `core/RenderContext.h`, `core/RenderWorld.h`, `core/RenderWorld.cpp`
- `core/RenderResources.h`, `core/RenderResources.cpp`
- `rhi/RHI.h`, `rhi/Vulkan/commands/CommandListVK.h`, `CommandListVK.cpp`
- `app/rendering_sandbox/main.cpp`

**A.1 adds:** `framegraph/FrameGraphPassNames.h` (rewritten), `framegraph/FrameGraphResourceNames.h` (new); passes + `RenderWorld` + `RasterFeature` routed through constants.

**A.2 adds:** `framegraph/BlackboardSlot.h` (new slots), `framegraph/FrameGraphBlackboard.h` (texture/buffer publish), `framegraph/FrameGraphResourceNames.h` (`kMainDepth`, `kPerFrameUniformBuffer`, `kPerViewUniformBuffer`), `core/RenderResources.h`/`.cpp` (standalone depth + shared-depth HDR RT + UBO placeholders), `core/RenderWorld.cpp` (four slot reservations + frame publish).

**A.3 adds:** `scene/DrawList.h` (`DrawLayer`, `DrawItem`, `draw_item_flags`).

**A.4 adds:** `scene/SceneDrawInstance.h`; `scene/IRenderSceneView.h`, `scene/RenderSceneView.h` (instance stream); `visibility/VisibilitySystem.cpp` (frustum + sortKey); `rhi/Vulkan/memory/DeletionQueueVK.cpp` (proc addr for AS destroy); `app/rendering_sandbox/main.cpp` (camera + test instances).

**A.5 adds:** `core/FrameViewUniforms.h`; `core/RenderResources.h`/`.cpp`, `core/RenderWorld.cpp`; `rhi/RHITypes.h` (`ComposeBootstrap`); `rhi/Vulkan/pipelines/VulkanResourceSpaces.*`, `rhi/Vulkan/core/DeviceVK.*`, `PipelineVK.cpp`, `PipelineVKMesh.cpp`, `CommandListVK.cpp`, `DescriptorSetVK.cpp`; `features/raster/RasterFeature.cpp`; `framegraph/passes/FullscreenHdrComposePass.*`.

**A.6 adds:** `framegraph/FrameGraphBarrierList.h` (`DedupeConsecutiveSameTargetState`), `framegraph/FrameGraph.h`/`.cpp` (`m_barrierPlans`, `MergeBarrierPlansAgainstTracker`, `Compile(..., blackboard)`), `framegraph/FrameGraphPass.h` (doc), `core/RenderWorld.cpp` (`Compile` passes `&m_blackboard`); `rhi/RHI.h` (`GetTextureTrackedState`); `rhi/Vulkan/core/DeviceVK.h`/`.cpp`.

**A.7 adds:** `framegraph/FrameGraph.cpp` / `FrameGraph.h` (intra-pass conflict warn; `WarnFirstMergedBarrierTextureUnknownLayout`, `WarnInterPassSimulatedLayoutGap`; merged asserts); `rhi/RHI.h` + `DeviceVK.*` (`GetTextureTrackedState(const ITexture*)`). *Follow-up (A.9):* dropped `WarnInterPassRawBarriersFullyMergedAway` (legitimate idempotent collects).

**A.8 adds:** `core/RenderResources.h`/`.cpp` (`Begin/EndUploadRecordingPhase`, debug depth, asserts, upload contract comment); `core/RenderWorld.h`/`.cpp` (`PrepareUploadPhaseResources`, `RecordFeatureUploads`, `UploadRecordingPhaseScope`).

**A.9 adds:** `framegraph/passes/FullscreenHdrComposePass.*` (no `CollectTextureBarriers` duplicate); `rhi/Vulkan/commands/CommandListVK.cpp` (conditional `BeginRenderingWithTarget` transitions); `framegraph/FrameGraph.cpp` / `FrameGraph.h`, `Render_steps_todo.md`, this file (smoke note).

**A.10 adds:** this file §A.10 (D1–D6 pass/partial notes); `Render_steps_todo.md` (tracking matrix **A** → Done).


---

## Files touched (M3)

- `framegraph/BlackboardWriterRegistry.h` (new)
- `framegraph/FrameGraphBlackboard.h`, `FrameGraphPass.h`, `FrameGraph.h`, `FrameGraph.cpp`
- `scene/IRenderSceneView.h`, `scene/RenderSceneView.h`, `scene/RenderSceneView.cpp`
- `core/RenderContext.h`, `core/RenderWorld.h`, `core/RenderWorld.cpp`
- `scheduler/RenderFeature.h`
- `app/rendering_sandbox/main.cpp`

---

### B.10 - Section B hardening (1.3 template, 2026-05-02)

Scope: milestone **M5 forward raster E2E** after B.1-B.9.

- **Dimension:** D1 (Frame graph integrity)  
  **Inspected:** `core/RenderWorld.cpp` (`RunFrame` remains upload -> schedule -> compile -> execute only), `framegraph/FrameGraph.cpp` (`Execute` runs pass barriers + pass bodies), `features/raster/RasterFeature.cpp` (`OnSchedulePasses` contributes clear/forward/blit as graph passes only).  
  **Result:** **pass**  
  **Notes:** No GPU write path in RasterFeature outside graph pass execution.

- **Dimension:** D3 (Feature isolation)  
  **Inspected:** include boundaries under `features/`, `framegraph/`, `scene/`; `RasterFeature` interactions are via `RenderContext` + `FrameGraphBlackboard` only.  
  **Result:** **pass**  
  **Notes:** No cross-feature direct calls; `RasterFeature` schedules only its own passes.

- **Dimension:** D7 (Capabilities & permutations)  
  **Inspected:** `features/raster/RasterFeature.cpp`, `shaders/ShaderLibrary.{h,cpp}`.  
  **Result:** **pass**  
  **Notes:** Permutations are declared in `OnRegister`; pipeline creation is centralized in `WarmUpPipelines`; pass `Execute` paths only bind cached pipelines (`TryGetPipeline` result).

- **Dimension:** D9 (Temporal ownership)  
  **Inspected:** `features/raster/` + `framegraph/passes/` for history allocation/previous-frame ownership.  
  **Result:** **pass (current scope)**  
  **Notes:** M5 raster path allocates no temporal history resources; no stray per-feature previous-frame textures/buffers introduced.

- **Dimension:** D10 (Observability)  
  **Inspected:** `framegraph/FrameGraph.cpp`, `core/GpuProfiler.cpp`, pass debug names.  
  **Result:** **pass**  
  **Notes:** Every graph pass is wrapped by `GpuProfiler::Scope` using pass debug names; Vulkan debug labels are emitted through command list hooks.

### B.11 - M5 closeout (2026-05-02)

- **M5 status:** **Done** (forward raster E2E delivered for current scope).
- **Section B status:** `B.1`-`B.4`, `B.6`-`B.11` complete; `B.5` remains optional and intentionally deferred.
- **Roadmap checklist:** P4/P5 treated as complete for the current implementation slice (upload phase + forward raster).

**Files touched (B.10/B.11 closeout):**

- `documentation/Render_Module/Render_check.md`
- `documentation/Render_Module/Render_steps_todo.md`
- `documentation/Render_Module/newrender_roadmap.md`
- `EngineSource/eyelib/rendering/framegraph/FrameGraphAttachmentOps.h` (Bx.1 kickoff)

*Update this file whenever a milestone or principle check changes.*

### Bx - Attachment ops contract closeout (2026-05-02)

Scope: Bx.1-Bx.6 (post-M5 stabilization).

- Added `framegraph/FrameGraphAttachmentOps.h` and pass-level optional contract `IFrameGraphPass::DescribeRenderAttachmentOps(...)`.
- Added `ICommandList::SetRenderTarget(rt, ops)` and Vulkan mapping in `CommandListVK::BeginRenderingWithTarget` to native dynamic-rendering load/store/clear fields.
- Migrated forward first-writer path to explicit begin-clear (HDR color + depth) in `ForwardOpaquePass`; removed default pre-forward clear pass scheduling from `RasterFeature`.
- Added `FrameGraph::Compile` debug validation for conflicting declared attachment ops on the same render-target textures (bounded warnings).
- Validation: Debug build successful for `EyelibRendering` and `OpenEyeRenderingSandbox`; sandbox smoke run confirms forward path still rendering (`visible=1`, first draw reached).

### P6 - Deferred layout decoupling update (2026-05-04)

- Added dedicated `RasterGBuffer` contracts (`PipelineLayoutKind::RasterGBuffer`, `ResourceSetType::RasterGBufferData`) to decouple GBuffer from forward layout usage.
- `GBufferPass` now binds `RasterGBufferData` (PerObject SSBO only); directional-light UBO binding was removed from the GBuffer set contract.
- Validation: Debug `OpenEyeRenderingSandbox` build passed after integration.