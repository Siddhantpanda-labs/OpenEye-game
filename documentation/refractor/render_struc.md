# Render Module Structure: Current vs Ultimate

This document records:

- the **current render-module design** (roles, ownership, connections),
- and the **ultimate target structure** for a clean Unreal-style architecture,
- with strict requirement: **no feature loss and no behavior regressions**.

---

## 1) Scope and Goals

Scope: `EngineSource/eyelib/render` (core render module), with direct orchestration focus on `renderer/RenderPipeline.`*.

Goals:

- Make ownership explicit.
- Reduce coupling and mixed responsibilities.
- Keep all existing features intact (Deferred/Forward/Forward+/Stereo/Post/Transparency/Skybox/EntityID/Outline/Debug/Streaming).
- Enforce one architecture contract across all passes.

---

## 2) Current Module Map (What Exists Today)

## 2.1 Top-level areas and role

- `rhi/`  
Backend abstraction + implementations (OpenGL/Vulkan/Diligent), command recording, descriptors, buffers/textures/pipelines.
- `framegraph/`  
Pass DAG, resource reads/writes, compile/execute flow, transient aliasing and resource state transitions.
- `renderer/`  
`RenderPipeline` orchestration and global render flow; `GBuffer` resource owner.
- `passes/`  
Classical render pass implementations and newly introduced `*PassSystem` wrappers.
- `shadows/`, `lighting/`, `post/`, `ibl/`, `reflection/`  
Domain subsystems used by pipeline.
- `resources/`, `materials/`, `submission/`, `2d/`  
Resource/material/submission helpers and 2D systems.

## 2.2 Key pass-related classes currently present

Classical pass implementations:

- `passes/GBufferPass.`*
- `shadows/ShadowPass.*`
- `passes/LightingPass.*`
- `passes/TransparencyPass.*`
- `passes/GpuCullPass.*`
- `lighting/LightCullPass.*`
- `post/PostProcessor.*`

New pass-system wrappers (in progress extraction):

- `passes/SkyboxPassSystem.*`
- `passes/PostProcessPassSystem.*`
- `passes/TransparencyPassSystem.*`

Primary orchestrator:

- `renderer/RenderPipeline.*`

---

## 3) Current Runtime Connections

## 3.1 Orchestration flow

`RenderPipeline` currently:

1. Prepares frame data from `IRenderSceneView`.
2. Builds/compiles frame graph per mode.
3. Executes pass lambdas (shadow, deferred lighting, transparency, post, etc.).
4. Manages many resource setup details (descriptor set prep, per-frame/per-draw rings).

## 3.2 Mode recipes (current behavior)

- Deferred: `GPU Cull -> Shadow -> GBuffer -> Lighting -> Transparency -> Skybox -> Post`
- Forward: `GPU Cull -> Shadow -> Forward Opaque -> Transparency -> Skybox -> Post`
- Forward+: `Shadow -> Forward+ Opaque -> Transparency -> Skybox -> Post`
- Stereo path has mode-specific graph/pass branches.

## 3.3 Current coupling status

- `RenderPipeline` still owns significant pass internals and backend conditionals.
- `*PassSystem` extraction has started, but core heavy logic remains hybrid:
  - Some behavior in classical pass classes,
  - some in `RenderPipeline`,
  - some in `*PassSystem` wrappers.

---

## 4) Current Design Risks (Technical Debt)

- **God orchestrator**: `RenderPipeline.cpp` still too large and cross-domain.
- **Mixed layers**: graph-facing orchestration and low-level pass details are still interwoven.
- **Backend leakage**: frequent Vulkan/OpenGL branching outside backend policy boundaries.
- **Hybrid pass model**: both classical pass classes and system wrappers coexist without full ownership boundary yet.
- **Editor leakage**: editor-facing functionality still coupled with runtime pipeline paths.

---

## 5) Ultimate Architecture (Unreal-Style Target)

## 5.1 Required architecture principles

- **One pass contract for all passes**:
  - `Build...Params`
  - `Build(...)`
  - `Execute(...)`
- **Mutation boundary**:
  - descriptor/buffer updates allowed in `Build(...)` only,
  - forbidden in `Execute(...)`.
- **Graph-facing ownership**:
  - frame graph only talks to `*PassSystem` owners.
- **Mode composition, not duplication**:
  - Forward/Forward+/Deferred differ by recipe wiring only, not duplicate pass implementations.
- **No random API naming**:
  - strict naming and lifecycle conventions.

## 5.2 Canonical layers and responsibilities

- **Layer A: RenderGraph Orchestrator**
  - composes recipes,
  - schedules systems,
  - no pass-specific descriptor details.
- **Layer B: Pass Systems (`*PassSystem`)**
  - sole owners for pass-level behavior,
  - expose explicit build/execute contracts,
  - hide backend policy details behind internal strategy.
- **Layer C: Resource Builders**
  - descriptor set ring allocation,
  - pass resource setup (UBO/SSBO/texture bindings),
  - immutable execute payload creation.
- **Layer D: Backend Policy**
  - backend-specific branches (Vulkan/OpenGL/Diligent),
  - no scattered backend checks in orchestration layer.
- **Layer E: Editor Overlay Systems**
  - entity ID, outline, debug lines isolated from runtime core.

---

## 6) Ultimate Naming and Ownership Contract

Mandatory naming:

- Class: `XxxPassSystem`
- Params: `XxxPassBuildParams`, `XxxPassExecuteParams`
- Methods: `BuildParams(...)`, `Build(...)`, `Execute(...)`

No ad-hoc verbs for pass lifecycle.

Every pass system is graph-facing owner:

- `GpuCullPassSystem`
- `ShadowPassSystem`
- `GBufferPassSystem`
- `DeferredLightingPassSystem`
- `ForwardOpaquePassSystem`
- `TransparencyPassSystem`
- `SkyboxPassSystem`
- `PostProcessPassSystem`
- `EditorOverlayPassSystem`
- `StreamingPassSystem`

Optional internal helper classes are allowed only as implementation details; they must not be graph-facing.

---

## 7) Ultimate Connection Diagram (Logical)

`RenderPipelineFacade`  
-> `RenderGraphOrchestrator`  
-> mode recipe builder (`DeferredRecipeBuilder` / `ForwardRecipeBuilder` / `ForwardPlusRecipeBuilder`)  
-> each graph node invokes one `*PassSystem` owner  
-> each `*PassSystem` uses resource builders and backend policy  
-> `Execute(...)` issues bind/draw/dispatch only.

Editor overlay runs as separate integration stage:

- `EditorOverlayPassSystem` reads produced runtime targets (HDR, EntityId buffers) and composites.

---

## 8) Feature Preservation Matrix (Must Not Break)

- Deferred rendering: keep GBuffer + lighting fidelity.
- Forward rendering: keep material variants, skinned/instanced support.
- Forward+: keep clustered/tiled path behavior.
- Stereo paths: keep left/right graph behavior.
- Transparency: keep sorting/blending correctness.
- Skybox/procedural sky: keep depth behavior and post compatibility.
- Post stack: keep SSR/SSAO/Bloom/Tonemap/FXAA/MotionBlur/Vignette/FilmGrain behavior.
- Entity ID + outline + debug lines: preserve editor workflows.
- NV12 streaming path: preserve export path behavior.
- Vulkan validation must remain clean (descriptor lifetime safety preserved).

---

## 9) Migration State (Current Snapshot)

Already extracted / wired:

- `renderer/graph/RenderGraphOrchestrator.*` (owns `FrameGraph`, `CompileIfNeeded` / `MarkNeedsCompile` / `Execute`); `renderer/graph/FrameGraphRecipeBuilders.*` (imports + `AddPassesFromRecipe` per mode; executors still from `RenderPipeline`)
- `SkyboxPassSystem`, `PostProcessPassSystem`, `TransparencyPassSystem`
- `DeferredLightingPassSystem`, `ForwardOpaquePassSystem`, `ShadowPassSystem`
- `EditorOverlayPassSystem`, `StreamingPassSystem`
- `PassResourceBuilder`, `RenderBackendPolicy`, `FrameRenderContext` + graph execution context
- **`GBufferPassSystem` (opaque body):** `GraphExecuteGBufferDeferred` builds `GBufferPassOpaqueParams` (`passes/GBufferPassOpaqueParams.h`) via `RenderPipeline::BuildGBufferOpaqueParams` and calls `GBufferPassSystem::RenderOpaqueGeometry` (implementation in `GBufferPassSystem.cpp`). `RenderPipeline.h` depends only on the params header, not the system class (forward-declared).
- **`ShadowPassSystem` (batched body):** `ShadowPassBuildParams` / `ShadowPassBatchedParams` in `passes/ShadowPassParams.h`; `RenderPipeline::BuildShadowBatchedParams` + `ShadowPassSystem::RenderShadowBatched` replace `RunShadowPassBatched` and the old callback-based `ShadowPassSystem::Execute`.
- **GPU cull prep:** `passes/GpuCullScenePrep.*` (append/finalize/configure + `RunGpuCullScenePrep*`); `passes/GpuCullPassSystem.*` owns `GpuCullPass`, the transform buffer, `GpuCullSceneScratchState`, and `PrepareFromVisibleOpaque` / `PrepareFromView`; `RenderPipeline` only assembles `GpuCullScenePrepUploadParams` (`BuildGpuCullScenePrepUploadParams`), owns `m_gpuCullPassSystem`, and `PrepareGpuCull*` delegates to the system.
- **Forward+ light cull:** `passes/LightCullPassSystem.*` owns `LightCullPass` lifecycle (`Initialize` / `Shutdown`); `RenderPipeline` keeps `ClusteredLightGrid`, point/spot/light-index buffers, and wires `Set*` + `Execute` on `GetPass()` for the GPU path.

Still to promote to **full** ownership (per §5 / ownership table):

- **`GBufferPassSystem`:** optional follow-ups — narrow `GBufferPassOpaqueParams` further.
- **Graph orchestration follow-up:** optional **host interface** for tests (swap `ExecutorFor*Pass`); stereo recipe + `ExecutorForForwardStereoSkyboxLeft/Right` live on `RenderPipeline` next to `GraphExecute*`.
- IBL / probe builders (`LightingEnvironmentBuilder`, `ProbeSelectionBuilder`) as in ownership table.

---

## 10) Completion Criteria (Ultimate Structure Achieved)

Architecture is considered complete when all are true:

- `RenderPipeline` is thin facade/composition root.
- Frame graph interacts only with pass systems, not legacy direct pass calls.
- Every pass follows explicit build/execute contract.
- Execute stage is mutation-free for descriptors/resources.
- Backend branching is localized to policy layer.
- Runtime and editor concerns are isolated by ownership.
- Full feature matrix passes and Vulkan validation remains clean.

---

## 11) Practical Rule for All Future Work

If a new render change introduces:

- ad-hoc pass function naming,
- pass logic bypassing `Build/Execute`,
- descriptor updates in execute stage,
- direct backend branching in orchestration layer,

it must be rejected or redesigned before merge.

---

## 12) Ownership Table (Current -> Target)

This is the execution control table for safe, scalable module refactor.

Legend:

- **Current Owner**: where logic primarily lives today.
- **Target Owner**: final single graph-facing owner.
- **Relation**: how it connects to graph/pipeline/other modules.
- **Migration Phase**: recommended sequence from `Render_refractor.md`.


| Domain                          | Current Owner                                                                                               | Target Owner                                                                          | Relation / Dependencies                                                           | Migration Phase         |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | ----------------------- |
| Pipeline orchestration          | `renderer/RenderPipeline.`* + `renderer/graph/RenderGraphOrchestrator.*` (owns `FrameGraph` + compile scheduling) | thin `RenderPipeline` facade + orchestrator-only graph layer                        | mode selection + pass wiring stay on pipeline; orchestrator holds FG + dirty state | Phase 5-6 (partial: orchestrator extracted) |
| Frame graph core                | `framegraph/FrameGraph.*`, `FrameGraphRecipe.*`                                                             | same (keep) + recipe builders per mode                                                | pass DAG, resource transitions, execution order; pass systems are graph callbacks | Keep + Phase 5          |
| Deferred geometry               | `passes/GBufferPass.*` + `GBufferPassSystem::RenderOpaqueGeometry` + `RenderPipeline::BuildGBufferOpaqueParams` | tighten params / optional `GpuCullPassSystem`                                       | reads draw lists + camera, writes GBuffer targets                               | Phase 2-4               |
| Deferred lighting               | `passes/LightingPass.*` + `RenderPipeline::Build/ExecuteDeferredLighting*`                                  | `passes/DeferredLightingPassSystem.*`                                                 | reads GBuffer/shadow/IBL, writes HDR                                              | Phase 2-4               |
| Forward opaque                  | `RenderPipeline::RenderForwardOpaque*` + shader cache/material bindings                                     | `passes/ForwardOpaquePassSystem.*`                                                    | shared by Forward + Forward+ via config/policy                                    | Phase 4                 |
| Transparency                    | `RenderPipeline::RenderForwardTransparent` + `passes/TransparencyPass.*` + `TransparencyPassSystem` wrapper | `passes/TransparencyPassSystem.*`                                                     | one graph-facing owner; backend branch internal only                              | In progress (Phase 2-4) |
| Skybox                          | `RenderPipeline::RenderSkyboxImpl/RenderProceduralSkyboxImpl` + `SkyboxPassSystem`                          | `passes/SkyboxPassSystem.*`                                                           | graph pass reads depth/context, writes HDR                                        | In progress (Phase 1-2) |
| Post processing                 | `post/PostProcessor.*` + `PostProcessPassSystem` wrapper                                                    | `passes/PostProcessPassSystem.*` (owner) + `PostProcessor` as internal implementation | final stage reads HDR/GBuffer, writes output RT                                   | In progress (Phase 1-2) |
| Shadow                          | `shadows/ShadowPass.*` + `ShadowPassSystem::RenderShadowBatched` + `RenderPipeline::BuildShadowBatchedParams` | optional further thin pipeline                                                       | reads `allOpaque`, writes cascade depth targets                                   | Phase 3-4               |
| GPU culling                     | `passes/GpuCullPassSystem.*` (owns `GpuCullPass`, transform buffer, scratch) + `GpuCullScenePrep.*`; `RenderPipeline` (`PrepareGpuCull*`, `BuildGpuCullScenePrepUploadParams`) | `passes/GpuCullPassSystem.*` (graph-facing owner; pipeline orchestrates only) | produces indirect draw list for downstream passes                                 | Phase 4 (partial: ownership consolidated) |
| Light culling (Forward+)        | `passes/LightCullPassSystem.*` (owns `LightCullPass`) + `lighting/LightCullPass.*`; grid + light SSBOs on `RenderPipeline` | `passes/LightCullPassSystem.*` (graph-facing owner for compute pass; optional policy façade later) | writes cluster/light index buffer for Forward+                                    | Phase 4 (partial: pass owner) |
| IBL build/use                   | `ibl/IBL.*` + `RenderPipeline::EnsureIBLFromFrameData`                                                      | `passes/resources/LightingEnvironmentBuilder.*`                                       | provides irradiance/prefilter/BRDF inputs for lighting/post                       | Phase 3-5               |
| Reflection probes               | `reflection/ReflectionProbeManager.*` + post/deferred lookups                                               | `passes/resources/ProbeSelectionBuilder.*`                                            | selects probe textures for deferred/post/sky reflection                           | Phase 3-5               |
| Editor entity-id                | `RenderPipeline::RenderEntityIDPass`                                                                        | `editor/EditorOverlayPassSystem.*`                                                    | offscreen ID RT for picking                                                       | Phase 3                 |
| Editor outline                  | `RenderPipeline::RenderOutlineFromIdBuffer`                                                                 | `editor/EditorOverlayPassSystem.*`                                                    | reads scene + ID buffers, composites outline                                      | Phase 3                 |
| Debug lines                     | `RenderPipeline::RenderDebugLines` + pending queue                                                          | `editor/EditorOverlayPassSystem.*`                                                    | overlay pass before post/final compose                                            | Phase 3                 |
| Streaming NV12                  | `RenderPipeline::EnsureStreamingNV12Pass/RunStreamingNV12Pass`                                              | `streaming/StreamingPassSystem.*`                                                     | compute conversion from RT to NV12                                                | Phase 3-4               |
| Descriptor rings/set allocators | mostly `RenderPipeline` member rings                                                                        | `resources/PassResourceBuilders.*`                                                    | per-frame/per-pass/per-draw set allocation and write ownership                    | Phase 2                 |
| Backend branching policy        | scattered in `RenderPipeline` and pass files                                                                | `backend/RenderBackendPolicy.*`                                                       | Vulkan/GL-specific decisions consumed by systems                                  | Phase 4-5               |


---

## 13) Connection and Dependency Table (Target)


| Layer                     | Allowed Dependencies                                                    | Forbidden Dependencies                                                       |
| ------------------------- | ----------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| `RenderPipeline` facade   | `RenderGraphOrchestrator`, high-level mode config, lifecycle services   | direct pass internals, descriptor updates, backend-specific code             |
| `RenderGraphOrchestrator` | framegraph core, pass-system interfaces, recipe builders                | direct shader/material draw code                                             |
| `*PassSystem`             | RHI, resource builders, backend policy, internal helper implementations | direct calls to unrelated pass systems (except explicit declared dependency) |
| resource builders         | RHI, frame context, pass-specific resource definitions                  | graph orchestration logic, scene traversal logic                             |
| backend policy            | RHI/backend capabilities, platform guards                               | pass orchestration state, scene business logic                               |
| editor overlay systems    | output RTs, editor data, RHI                                            | runtime core pass ownership                                                  |


---

## 14) Safe Migration Order (Whole Module)

1. Keep framegraph core stable (no behavior change).
2. Complete wrappers to full ownership for already-started systems:
  - `SkyboxPassSystem`, `PostProcessPassSystem`, `TransparencyPassSystem`.
3. Extract `DeferredLightingPassSystem` and `ShadowPassSystem`.
4. Extract `ForwardOpaquePassSystem` + shared forward/forward+ policy.
5. Promote cull systems (`GpuCull`, `LightCull`) to standard pass-system contracts — pass + buffer ownership for GPU cull; `LightCullPass` lifetime under `LightCullPassSystem`; remaining work is unified `Build/Execute` naming and graph-only dispatch where applicable.
6. Move editor-only paths to `EditorOverlayPassSystem`.
7. Move streaming path to `StreamingPassSystem`.
8. Split graph orchestration from `RenderPipeline`.
9. Final naming normalization + cleanup (no functional changes).

---

## 15) Scalability Rules (Enforced)

- One graph-facing owner per domain (no split ownership).
- New features must extend existing system contracts before creating a new system.
- Any backend-specific logic must be policy-injected, not orchestration-embedded.
- If a pass needs new resource bindings, update its `BuildParams` and builder only.
- Feature additions must update this ownership table before implementation starts.

