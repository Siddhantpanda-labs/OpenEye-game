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

---

## 11) Immediate Next Step

Start with **Phase 1 + Skybox extraction first** (low risk, high structural win), then continue with post/transparency in that order to steadily reduce `RenderPipeline` size while preserving runtime behavior.

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
struct FrameRenderContext {
    // immutable per frame
    rhi::IDevice* device;
    rhi::ICommandList* cmd;
    const RenderFrameData* frameData;
    uint32_t frameIndex;
    uint32_t frameNumber;
    uint32_t width;
    uint32_t height;
};

struct PassResourceContext {
    // descriptor set rings, reusable buffers, transient handles, etc.
    // setup stage is allowed to mutate this.
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
They are recipe assemblers:

- `DeferredPipelineRecipeBuilder`
  - `GPUCull -> Shadow -> GBuffer -> DeferredLighting -> Transparency -> Skybox -> Post`

- `ForwardPipelineRecipeBuilder`
  - `GPUCull -> Shadow -> ForwardOpaque -> Transparency -> Skybox -> Post`

- `ForwardPlusPipelineRecipeBuilder`
  - `Shadow -> ForwardPlusPrepare -> ForwardOpaque(config=ForwardPlus) -> Transparency -> Skybox -> Post`

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

1. Introduce `FrameRenderContext` and `PassResourceContext`.
2. Add `IPassSystem` and first extracted concrete system (`SkyboxPassSystem`).
3. Convert graph lambdas to `Build + Execute` flow with explicit params.
4. Extract `DeferredLightingPassSystem`, `TransparencyPassSystem`, `ForwardOpaquePassSystem`.
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

