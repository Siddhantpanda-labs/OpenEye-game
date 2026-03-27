# Render Pipeline Refactor Plan (Unreal-Style, Zero Regression)

## 1) Goals and Non-Negotiables

### Primary goals
- Make rendering architecture clean, scoped, and modular.
- Keep behavior/output identical while refactoring.
- Enforce strict setup/build vs execute separation.
- Remove editor/tooling leakage from core runtime pipeline.
- Reach an Unreal-style pass architecture: explicit params, immutable execute, predictable resource lifetime.

### Non-negotiables
- No feature loss (deferred, forward, forward+, stereo, post, skybox, transparency, entity-id, outline, debug lines, streaming NV12).
- No Vulkan validation regressions.
- No runtime output regressions (frame equivalence within expected floating error).
- No temporary hacks merged as final architecture.

---

## 2) Current Problems (Observed)

- `RenderPipeline` is a God object (orchestrator + pass implementation + editor overlays + streaming utility).
- Core runtime and editor-facing features are mixed in same class/file.
- Backend branching (Vulkan/OpenGL behavior checks) is distributed inside pass logic.
- Some helper code still owns resource mutation concerns that should be isolated in setup modules.
- Too much pass-specific logic in one translation unit reduces clarity and testability.

---

## 3) Target Architecture (End State)

## 3.1 High-level layers
- **Render Orchestrator Layer**: builds frame graph, schedules pass execution, owns frame context only.
- **Pass Systems Layer**: each pass family in dedicated module/class.
- **Resource Setup Layer**: per-pass builders that allocate/fill descriptors/UBOs/SSBOs before execute.
- **Execute Layer**: bind + draw/dispatch only; no descriptor writes.
- **Editor Overlay Layer**: entity-id, outline, debug lines separated from core pipeline.
- **Backend Adapter Layer**: backend-specific details isolated behind small interfaces/helpers.

## 3.2 Module split (proposed)
- `renderer/graph/RenderGraphOrchestrator.*`
- `renderer/passes/ShadowPassSystem.*`
- `renderer/passes/DeferredLightingPassSystem.*`
- `renderer/passes/ForwardOpaquePassSystem.*`
- `renderer/passes/TransparencyPassSystem.*`
- `renderer/passes/PostProcessPassSystem.*`
- `renderer/passes/SkyboxPassSystem.*`
- `renderer/editor/EditorOverlayPassSystem.*` (entity-id, outline, debug)
- `renderer/streaming/StreamingPassSystem.*` (NV12)
- `renderer/resources/PassResourceBuilders.*` (descriptor/ubo setup)
- `renderer/backend/RenderBackendPolicy.*` (backend-specific glue)

`RenderPipeline` remains as facade/composition root, not implementation dump.

---

## 4) Scope and Ownership Rules

### 4.1 Core pipeline scope
- Frame lifecycle, frame data, graph compilation and execute.
- Runtime rendering only.

### 4.2 Editor scope
- Selection/picking entity-id pass.
- Outline composite pass.
- Debug line overlay.

### 4.3 Hard rule: mutation boundary
- **Allowed in setup/builders only**: `UpdateTexture`, `UpdateStorageBuffer`, `UpdateUniformBuffer`, pass-set preparation.
- **Forbidden in execute**: descriptor mutation, resource reconfiguration.
- Execute phase = bind pipeline/sets + push constants + draw/dispatch.

---

## 5) Refactor Strategy (Incremental, Safe)

## Phase 0 - Baseline and Guardrails
- Freeze current behavior with baseline captures:
  - deferred, forward, forward+, stereo, transparency-heavy, post-heavy, editor selection, debug lines.
- Add automated validation checklist:
  - Vulkan validation clean.
  - Pass presence/ordering expected.
  - Descriptor update-while-in-use errors absent.
- Add lightweight perf counters snapshot for comparison.

## Phase 1 - Extract Pass Interfaces (No behavior change)
- Introduce pass system interfaces and move code mechanically by responsibility.
- Keep function bodies identical during first move.
- `RenderPipeline` calls extracted modules.

Deliverable:
- Same output, reduced `RenderPipeline` responsibilities.

## Phase 2 - Extract Resource Builders
- Create dedicated setup/build structs per pass:
  - `ShadowBuildParams`, `DeferredLightingBuildParams`, etc.
- Move all descriptor/UBO/SSBO fill logic to builders.
- Execute functions accept prepared immutable params and only bind/draw.

Deliverable:
- Enforced setup/execute contract across all pass systems.

## Phase 3 - Separate Editor Overlay Stack
- Move:
  - `RenderEntityIDPass`
  - `RenderOutlineFromIdBuffer`
  - `RenderDebugLines`
  - pending debug line queue handling
  into `EditorOverlayPassSystem`.
- Core runtime path must not reference editor-only entities directly.

Deliverable:
- No editor leakage in core render orchestrator.

## Phase 4 - Backend Isolation
- Introduce backend policy/helper objects for API differences:
  - typed push path, resource-space binding assumptions, fallback behavior.
- Remove scattered `if (backend == Vulkan)` from pass flow where possible.

Deliverable:
- Cleaner generic pass logic, backend details localized.

## Phase 5 - FrameGraph Contract Hardening
- Standardize every pass as:
  - `BuildPassParams(frameCtx, sceneCtx, resources)` then `Execute(params)`.
- Explicit resource declarations and dependencies per pass.
- Optional: central pass registry/factory for readability and scalability.

Deliverable:
- Strong Unreal-style pass contract and graph readability.

## Phase 6 - Final Cleanup
- Remove dead code and temporary migration branches.
- Normalize naming (`Build*`, `Prepare*`, `Execute*`).
- Add architecture docs with ownership matrix and extension guide.

Deliverable:
- Stable, maintainable, clean architecture baseline.

---

## 6) Detailed Work Breakdown

## 6.1 Core extraction order (recommended)
1. `SkyboxPassSystem` (smallest risk, easy isolation)
2. `PostProcessPassSystem`
3. `TransparencyPassSystem`
4. `DeferredLightingPassSystem`
5. `ForwardOpaquePassSystem` + forward+ opaque
6. `ShadowPassSystem`
7. `EditorOverlayPassSystem`
8. `StreamingPassSystem`

## 6.2 Shared services to introduce
- `FrameRenderContext` (immutable per frame)
- `PassResourceContext` (descriptor pools/rings, buffers, textures)
- `RenderFeatureFlags` (capability gates)
- `RenderDebugHooks` (logs/profiler hooks)

## 6.3 Descriptor lifetime policy
- Per-frame + per-pass + per-draw ring strategy stays mandatory.
- No shared mutable descriptor sets across active pass recordings.
- Builders may allocate/rotate sets; execute consumes finalized handles.

---

## 7) Regression Prevention Plan

## 7.1 Test matrix
- Modes: Deferred / Forward / Forward+
- Cameras: Mono / Stereo
- Scene types:
  - static opaque
  - skinned + instanced
  - transparency-heavy
  - IBL on/off
  - shadow on/off
- Editor features:
  - entity ID picking
  - outline
  - debug lines
- Streaming path:
  - NV12 pass on/off

## 7.2 Validation gates per phase
- Build success (Debug at minimum).
- Vulkan validation: zero descriptor lifetime errors.
- Visual compare against baseline scenes.
- Performance sanity (no major CPU/GPU regressions from orchestration changes).

---

## 8) Coding Standards for This Refactor

- Keep every refactor commit behavior-preserving unless explicitly marked feature change.
- Prefer move/rename first, logic change second (small reviewable diffs).
- Keep APIs explicit with params structs; avoid hidden global mutation.
- No new string-based uniform routing in Vulkan path.
- Keep push constants through typed ABI writers.
- Minimize logs in hot paths; keep warnings actionable and non-spammy.

---

## 9) Definition of Done (Unreal-Style Milestone)

This refactor is considered complete when:
- `RenderPipeline` is a thin orchestrator/facade.
- Core runtime has no editor/tooling leakage.
- Every pass follows `Build*Params -> Execute`.
- Execute paths are mutation-free for descriptors/resources.
- Backend divergence is isolated, not scattered.
- Vulkan validation remains clean under full test matrix.
- Visual parity and feature parity are confirmed.

---

## 10) Execution Checklist (Track Progress)

- [ ] Baseline capture + validation checklist committed.
- [ ] Pass system interfaces introduced.
- [ ] Skybox extracted.
- [ ] Post-processing extracted.
- [ ] Transparency extracted.
- [ ] Deferred lighting extracted.
- [ ] Forward/Forward+ opaque extracted.
- [ ] Shadow extracted.
- [ ] Editor overlay system extracted.
- [ ] Streaming NV12 extracted.
- [ ] Shared resource builders unified.
- [ ] Backend policy abstraction introduced.
- [ ] `RenderPipeline` reduced to orchestrator role.
- [ ] Full regression matrix passed.
- [ ] Final architecture docs updated.

Granular done/in-progress items (post graph, deferred lighting `BuildParams`, `GraphExecute*`, `FrameRenderContext`) are tracked in **Implementation Tracker (Live)** at the end of this document.

---

## 11) Immediate Next Step

Continue **Phase 1–2**: push remaining descriptor/build work into pass-system `BuildParams` where it still lives in `RenderPipeline`, adopt `FrameRenderContext` inside `GraphExecute*` and pass systems for a single per-frame input surface, and keep graph recipes limited to **scene passes** (post stays outside the graph; see §14).

---

## 12) Canonical Pass and Graph System Design (Generic + Explicit)

This section defines the exact class model and naming contract to avoid random one-off APIs.

## 12.1 Naming Contract (strict)

- **System classes**: `*PassSystem` (example: `DeferredLightingPassSystem`)
- **Build-time params**: `*BuildParams`
- **Execute-time params**: `*ExecuteParams`
- **Shared per-frame context**: `FrameRenderContext`
- **Shared resource access**: `PassResourceContext`
- **Graph nodes**: `*PassNode` (optional if we use struct-only node descriptors)
- **No ad-hoc names** like `DoThingNow`, `RunX2`, `TempPass`.

Method naming must be one of:
- `Build(...)`
- `Execute(...)`
- `DeclareResources(...)`
- `Validate(...)` (optional, debug/dev only)

No extra verbs unless justified and documented.

## 12.2 Base Interfaces

```cpp
// Implemented: `EngineSource/eyelib/render/renderer/FrameRenderContext.h`
struct FrameRenderContext {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    const RenderFrameData* frameData = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameIndex = 0;
};

struct PassResourceContext {
    // Placeholder until descriptor rings move behind builders; setup may mutate later.
};

struct IPassSystem {
    virtual ~IPassSystem() = default;
    virtual const char* GetName() const = 0;
};
```

## 12.3 Per-pass contract (all passes follow this)

```cpp
struct DeferredLightingBuildParams { /* inputs from graph + frame */ };
struct DeferredLightingExecuteParams { /* immutable prepared resources */ };

class DeferredLightingPassSystem final : public IPassSystem {
public:
    const char* GetName() const override { return "DeferredLighting"; }

    DeferredLightingExecuteParams Build(
        const FrameRenderContext& frameCtx,
        PassResourceContext& resourceCtx,
        const DeferredLightingBuildParams& in);

    void Execute(
        const FrameRenderContext& frameCtx,
        const DeferredLightingExecuteParams& prepared);
};
```

Hard rule:
- `Build(...)` may allocate/update descriptor sets and buffers.
- `Execute(...)` must not call descriptor update APIs.

## 12.4 Graph-side contract

Each graph pass entry contains:
- pass name (stable),
- resource reads/writes,
- lambda/function that:
  1) builds `*BuildParams`
  2) calls system `Build(...)`
  3) calls system `Execute(...)`

```cpp
struct RenderGraphPassDesc {
    std::string name;
    std::vector<ResourceHandle> reads;
    std::vector<ResourceHandle> writes;
    std::function<void(const FrameRenderContext&, PassResourceContext&)> run;
};
```

This keeps graph generic and pass implementations reusable.

---

## 13) Concrete Pass Systems and Responsibilities

## 13.1 `DeferredLightingPassSystem`

- Inputs: gbuffer textures, shadow texture/buffer, scene lights buffers, IBL/probe textures, camera.
- Build:
  - select/acquire per-pass descriptor set
  - update bindings once
  - prepare immutable execute payload
- Execute:
  - bind RT + pipeline + descriptor sets
  - draw fullscreen

## 13.2 `ForwardOpaquePassSystem`

- Inputs: visible opaque draw list (or gpu-cull output), camera, scene/lighting resources, material resources.
- Build:
  - prepare pass-level shared sets
  - prepare per-draw sets/material bindings via ring allocators
- Execute:
  - iterate buckets, bind and submit draw commands only

Forward and Forward+ should reuse this system with different light/grid providers via config in build params.

## 13.3 `TransparencyPassSystem`

- Inputs: visible transparent draw list, depth source, camera, scene lighting handles.
- Build:
  - set up transparent pass descriptors and per-draw descriptors
- Execute:
  - bind target/viewport and draw transparent objects

Must be shared by Deferred/Forward/Forward+ recipes.

## 13.4 Other systems (same contract)

- `ShadowPassSystem`
- `SkyboxPassSystem`
- `PostProcessPassSystem`
- `EditorOverlayPassSystem` (entity-id, outline, debug lines)
- `StreamingPassSystem`

---

## 14) Pipeline Recipe Model (Forward / Forward+ / Deferred)

Pipelines are not separate implementations of the same pass logic.
They are recipe assemblers.

**Implemented today** (`RenderGraphRecipes.cpp`): recipes list **scene passes only**. Final **post-processing** (tone map, bloom, etc.) is **not** a graph node; it runs **once** at the end of `RenderPipeline::Render` after entity-id, callbacks, and debug draws, so post cannot run twice.

- **Deferred**: `GPU Cull -> Shadow -> GBuffer -> Copy depth for occlusion -> Lighting -> Transparency -> Skybox`
- **Forward**: optional `GPU Cull`, then `Shadow -> Forward Opaque -> Copy depth for occlusion -> Transparency -> Skybox`
- **Forward+**: `Shadow -> Forward+ Opaque -> Transparency -> Skybox`

**Target naming** (same ordering idea as above; post remains outside graph):

- `DeferredPipelineRecipeBuilder`: scene chain as above; `Post` after overlays in the pipeline.
- `ForwardPipelineRecipeBuilder`: scene chain as above; `Post` after overlays.
- `ForwardPlusPipelineRecipeBuilder`: scene chain as above; `Post` after overlays.

Only recipe differs; pass systems stay shared.

---

## 15) API Hygiene Rules (to keep it clean long term)

- New pass must introduce both `*BuildParams` and `*ExecuteParams`.
- No pass can directly fetch random globals from `RenderPipeline`.
- No pass execute can mutate descriptor resources.
- No editor-facing API in core runtime pass systems.
- Every pass name and resource binding must be deterministic and documented.

---

## 16) Migration Steps for This New Contract

1. Introduce `FrameRenderContext` and `PassResourceContext`. **Done (first slice):** structs live in `FrameRenderContext.h`; `PassResourceContext` is a placeholder until descriptor rings move behind builders.
2. Add `IPassSystem` and first extracted concrete system (`SkyboxPassSystem`).
3. Convert graph lambdas to `Build + Execute` flow with explicit params. **In progress:** frame-graph execution is routed through `RenderPipeline::GraphExecute*` helpers (deferred / forward / forward+), replacing large inline lambdas in `getExecutor`.
4. Extract `DeferredLightingPassSystem`, `TransparencyPassSystem`, `ForwardOpaquePassSystem`. **In progress:** `DeferredLightingPassSystem::BuildParams` owns lighting-pass descriptor updates that previously sat in `BuildDeferredLightingPassParams`; other systems partially wired.
5. Move editor/picking/outline/debug into `EditorOverlayPassSystem`.
6. Keep `RenderPipeline` as composition root + recipe selection only.

---

## 17) Architecture Law (Must Follow)

This is mandatory for all current and future render code.

- **Generic and explicit first**: pass APIs must use shared contracts (`Build`, `Execute`, explicit params, declared resources).
- **Specific cases are branches of the generic contract**: backend/mode/special behavior is configured via params/policy inside the same pass flow.
- **No random one-off APIs**: do not add ad-hoc function names for behavior that fits existing pass contracts.
- **Uniform lifecycle for every pass**: `Build*Params -> Build(...) -> Execute(...)`.
- **No hidden mutation in execute**: descriptor/resource writes are setup-only.
- **Any exception requires written justification** in this doc (what, why unavoidable, scope, and rollback plan).

### 17.1 PR/Review Gate

A render change must be rejected if any of the following is true:
- Introduces pass logic that bypasses `Build/Execute` contract.
- Adds duplicate mode-specific pass implementation instead of param/policy branching.
- Introduces non-standard naming for pass responsibilities.
- Mutates descriptors/resources inside execute stage.

### 17.2 Quick litmus test

Before merging a new render function, answer:
1. Does it map to `Build` or `Execute` clearly?
2. Are inputs/outputs explicit params (no hidden globals)?
3. Is special-case behavior a branch/policy, not a new ad-hoc path?
4. Is naming aligned with this contract?

If any answer is "no", redesign before merge.

---

## Implementation Tracker (Live)

Update this block at the end of each refactor step.

- [x] Pass system extraction started.
- [x] `SkyboxPassSystem` introduced and wired through `RenderPipeline`.
- [x] `PostProcessPassSystem` introduced and wired through `RenderPipeline`.
- [x] `TransparencyPassSystem` extraction (ownership promotion started).
- [x] `DeferredLightingPassSystem` extraction (ownership promotion started).
- [x] `ForwardOpaquePassSystem` extraction (ownership promotion started; graph + non-graph forward path wired).
- [x] `ShadowPassSystem` extraction (ownership promotion started).
- [x] `EditorOverlayPassSystem` extraction (outline + entity-id + debug-lines ownership promotion started).
- [x] `StreamingPassSystem` extraction (wrapper stage).
- [x] Render graph recipe builder split from `RenderPipeline` (first slice: Deferred/Forward/Forward+ recipes extracted).
- [x] **Post vs frame graph:** `"Post"` removed from all recipes; a **single** post pass runs at the end of `RenderPipeline::Render` after overlays (fixes duplicate post).
- [x] **Deferred lighting build ownership:** G-buffer / forward frame / lighting-pass descriptor updates and `LightingPass::SetResourceSpaceSets` moved into `DeferredLightingPassSystem::BuildParams` (pipeline still prepares frame data, IBL/cascade, scene lighting buffer, and acquires the deferred lighting draw set).
- [x] **`FrameRenderContext` / `PassResourceContext`:** added in `renderer/FrameRenderContext.h`; `RenderPipeline::GetFrameRenderContext()` fills device, cmd, frame data, size, frame index.
- [x] **Graph execution helpers:** `RenderPipeline::GraphExecuteGpuCull`, `GraphExecuteShadow`, `GraphExecuteGBufferDeferred`, `GraphExecuteCopyDepthOcclusionDeferred`, `GraphExecuteDeferredLighting`, `GraphExecuteTransparency`, `GraphExecuteSkybox`, `GraphExecuteForwardOpaque`, `GraphExecuteCopyDepthOcclusionForward`, `GraphExecuteForwardPlusOpaque` — frame graph `getExecutor` maps pass names to these (no duplicate giant lambdas).
- [x] **Explicit execute inputs cleanup:** deferred-lighting and transparency legacy execute paths now consume the command list passed through params/call-site (`cmd` / `p.build.cmd`) instead of re-fetching via `RenderContext::FromDevice(...)` inside execute callbacks.
- [x] **Shadow build ownership cleanup:** shadow setup now flows as `BuildShadowPassParams(cmd, frameData, cam) -> ShadowPassBuildParams -> ShadowPassSystem::BuildParams/Execute`, so pass enablement and command-list wiring are defined in build data before execute.
- [x] **Post-process execute API cleanup:** removed unused `cmd` argument from `RenderPipeline::ExecutePostProcessPass`; post now runs from explicit prepared params only (`BuildPostProcessPassParams -> PostProcessPassSystem::BuildParams/Execute`).
- [x] **FrameRenderContext signature consolidation (first slice):** deferred-lighting and transparency builders now take `FrameRenderContext`; `cmd/width/height` are carried in prepared params and execute no longer takes separate command-list arguments for those paths.
- [x] **FrameRenderContext signature consolidation (shadow):** shadow builder now takes `FrameRenderContext` as input, keeping `cmd` + per-frame data under the same explicit context contract before `ShadowPassSystem::BuildParams/Execute`.
- [x] **Shadow execute command-flow cleanup:** `RunShadowPassBatched` now uses its explicit `cmd` parameter for single-draw fallback too (no internal command-list re-fetch in that branch).
- [x] **Stereo graph executor cleanup:** `BuildForwardStereoGraph` pass executors (shadow/forward-opaque/transparency) now read frame data + command list via `FrameRenderContext` instead of per-lambda `RenderContext::FromDevice(...)` fetches.
- [x] **Explicit cmd propagation cleanup (runtime paths):** in stereo execute, forward transparent fallback, and debug overlay calls, `RenderContext` is now constructed from in-scope explicit `cmd` (not refetched from device) where command list ownership already exists.
- [x] **Explicit cmd propagation cleanup (overlay/light-cull slice):** Forward+ light-cull dispatch context now uses function-scope `cmd`, and debug-line overlay callbacks build `RenderContext` from explicit command ownership (`cmd` or `GetRHICommandList`) instead of `RenderContext::FromDevice(...)`.
- [x] **RenderPipeline `RenderContext::FromDevice` sweep complete:** all remaining occurrences in `RenderPipeline.cpp` were replaced with explicit `RenderContext{ device, cmd }` construction (or direct `m_device->GetImmediateCommandList()` in const context) to keep command ownership explicit.
- [x] **FrameRenderContext signature consolidation (post):** `BuildPostProcessPassParams` now takes `FrameRenderContext` (plus scene/camera inputs), and post IBL sourcing no longer depends on implicit `m_frameData` at that boundary.
- [x] **Deferred build-stage ownership expanded:** forward-frame UBO upload (view/proj/viewProj/prev matrices) moved from `RenderPipeline::BuildDeferredLightingPassParams` into `DeferredLightingPassSystem::BuildParams`, keeping this resource mutation inside pass build stage.
- [x] **Transparency build ownership expanded:** transparency path selection and pass resources (`device`, `hdr/gbuffer RT`, `preferVulkanPath`) are now filled in `BuildTransparencyPassParams`, so execute consumes prepared inputs instead of recomputing pipeline-state decisions.
- [x] **Deferred execute contract cleanup:** `DeferredLightingPassSystem::Execute` now consumes only prepared execute params (device/cmd/lightingPass/gbuffer moved into build payload), removing extra runtime arguments from the execute call.
- [x] **Post-process execute contract cleanup:** `PostProcessPassSystem::Execute` now takes only `PostProcessPassExecuteParams`; `PostProcessor*` and `GBuffer*` are carried in build/execute params (filled by pipeline before `BuildParams`).
- [x] **Skybox execute contract cleanup:** `SkyboxPassSystem::ExecuteCubemap` / `ExecuteProcedural` take only `SkyboxPassExecuteParams`; `Shader*` is in `SkyboxPassBuildParams` (pipeline sets cubemap vs procedural shader before `BuildParams`).
- [x] **Forward opaque execute contract cleanup:** `ForwardOpaquePassSystem::Execute` now takes only `ForwardOpaquePassExecuteParams`; draw callback is carried in `ForwardOpaquePassBuildParams` and invoked from execute using prepared cmd/frame/camera/drawables.
- [x] **Shadow execute contract cleanup:** `ShadowPassSystem::Execute` now takes only `ShadowPassExecuteParams`; draw callback is carried in `ShadowPassBuildParams` and invoked from execute with prepared cmd/frame/camera.
- [x] **Transparency execute contract cleanup:** `TransparencyPassSystem::Execute` now takes only `TransparencyPassExecuteParams`; Vulkan/legacy draw callbacks and `ShaderCache*` are carried in `TransparencyPassBuildParams` (pipeline fills them before `BuildParams`).
- [x] **Editor overlay execute contract cleanup:** `EditorOverlayPassSystem::ExecuteEntityId`, `ExecuteOutline`, and `ExecuteDebugLines` now each take only prepared execute params; draw/bind/push callbacks are carried in each overlay build params object before `Build*Params`.
- [x] **Streaming execute contract validation:** `StreamingPassSystem::ExecuteNv12` already follows the prepared execute-params-only contract (`BuildNv12Params` + `ExecuteNv12(params)`), so no execute-time callback side channel remains.
- [x] **Backend policy first slice:** added `backend/RenderBackendPolicy.*` and switched `RenderPipeline` backend decisions (camera adjustment, typed push-constant gating, transparency Vulkan-path preference, skybox/streaming/editor Vulkan flags) to policy helpers instead of inline backend checks.
- [x] **Pass resource-builder first slice:** extracted forward/deferred descriptor-set lifetime + per-frame draw-set acquisition into `resources/PassResourceBuilders.*`; `RenderPipeline` now delegates `EnsureForwardResourceSets` and all `Acquire*Set` calls to this builder while keeping pass bindings/behavior unchanged.
- [x] **Naming + API-doc hygiene pass (enforcement):** renamed resource-builder symbols to generic/professional names (`PassResourceBuilder`, `AcquireMaterialDrawSet`, `AcquirePassDrawSet`, `AcquireSceneDrawSet`, `AcquireDeferredLightingSet`), aligned pipeline wrappers to the same vocabulary, and added concise `@param/@return` API comments in `resources/PassResourceBuilders.h` and `backend/RenderBackendPolicy.h`.
- [x] **Variable naming + duplication cleanup:** normalized internal variable names in `PassResourceBuilders.cpp` (`ResourceSetType`, `frameUboDesc`, `bonesBufferDesc`, `frameSlot`) and replaced repeated dynamic-set acquire blocks with one generic helper (`AcquireDynamicSet`) to keep implementation generic and non-duplicative.
- [x] **Resource-builder field naming normalization:** removed forward-biased field names in `PassResourceBuilders` state (`CurrentFrameResources` + member pools/cursors) to generic resource names (`frameSet`, `passSet`, `materialSet`, `sceneSet`, `frameUbo`, `bonesBuffer`, etc.); `RenderPipeline` mapping updated accordingly.
- [x] **Material default-binding ownership moved:** default material fallback textures + material-set update logic moved from `RenderPipeline` into `PassResourceBuilder` (`UpdateMaterialSetWithDefaults` + internal default-texture lifecycle), further thinning pipeline responsibility without changing draw-path behavior.
- [x] **Forward pass-state write extraction:** descriptor/UBO write logic used by `RenderPipeline::BuildForwardPassSharedState` moved into `resources/ForwardPassStateWriter.*` with explicit `ForwardPassStateWriteParams`, reducing `RenderPipeline` implementation scope to orchestration/input assembly.
- [x] **Forward shared-state writer relocation/rename:** moved pass-state writer to `passes/common/ForwardSharedStateWriter.*` and renamed API to `ForwardSharedStateWriteParams` / `WriteForwardSharedState`, clarifying ownership as forward-family shared pass utility (separate from deferred lighting system).
- [x] **Forward environment resolve extraction:** probe/IBL/shadow/light-space selection for forward-family shared state moved into `ResolveForwardSharedEnvironment` (`passes/common/ForwardSharedStateWriter.*`), leaving `RenderPipeline::BuildForwardPassSharedState` as explicit input assembly + dispatch.
- [x] **Forward-path resolver reuse cleanup:** `RenderForwardOpaqueForCamera`, `RenderForwardTransparent`, and `RenderForwardPlusOpaqueBatched` now reuse `ResolveForwardSharedEnvironment` for IBL availability (`u_UseIBL`) instead of repeating local probe/IBL checks.
- [x] **Probe lookup dedup cleanup:** added `ResolveBestReflectionProbe` in `passes/common/ForwardSharedStateWriter.*` and replaced repeated closest/global probe lookup branches in `RenderPipeline` (persistent shader-read prep + post-process probe selection paths).
- [x] **Forward environment-params dedup cleanup:** added one local `RenderPipeline.cpp` helper (`ResolveForwardSharedEnvironmentForPipeline`) to assemble shared forward environment resolve inputs once and reuse it across forward opaque/transparent/forward+ plus `BuildForwardPassSharedState`.

### Current status note
- Current stage: **Phase 1 (pass interface extraction)** with **early Phase 2** work on deferred lighting build ownership and graph executor structure.
- Constraint: **zero behavior regression** while moving logic behind explicit pass system contracts.
- **Not done yet:** `GetFrameRenderContext()` is not required at all graph call sites; full `Build -> Execute` split with mutation-free execute for every pass; `RenderBackendPolicy`; thin orchestrator-only `RenderPipeline`.

