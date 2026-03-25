# Vulkan Fix Plan (Industry-Level Robustness)

This document defines the production path for Vulkan rendering correctness and long-term maintainability, aligned with patterns used in robust engines (Unity/Unreal style architecture) and informed by reference behavior in `Aespinosa_vulkan`.

## Current Status (What Is Already Improved)

- Forward draw path is now confirmed alive:
  - visible drawables are collected,
  - shaders/pipelines are valid,
  - descriptor sets are acquired,
  - mesh handles submit draw calls.
- Skybox visibility path was repaired (depth compare handling and clear-path cleanup).
- Vulkan dynamic state support is detected and logged (`VK_EXT_extended_dynamic_state`).
- Forward diagnostics now report drop-off reasons (shader/pipeline/descriptor/mesh handle counters).
- CPU frustum culling was re-enabled with raw camera use for culling-side math.

## Remaining Problem Statement

Meshes can still be invisible even when forward submits valid draws. This means the issue is now likely in GPU visibility stages (clip/depth/convention), not in scene collection or draw submission.

## Architectural Target (What “Done” Means)

1. One canonical camera/projection convention per backend.
2. No hidden renderer-side projection hacks as permanent behavior.
3. Pass state deterministic and observable.
4. No silent no-op behavior in critical backend APIs.
5. Reproducible verification via logs/tests (not manual guesswork).

## Phased Execution Plan

### Phase 1: Projection Ownership and Clip-Space Contract

Goal: camera projection is produced in a backend-correct way at source; renderer does not permanently patch matrix semantics.

Tasks:
- Introduce explicit camera projection contract for Vulkan (`RH + ZO + explicit Y policy`) and document it in code.
- Migrate camera producers (editor/player/api paths) to the contract.
- Keep renderer boundary adjustment only as temporary compatibility path; remove once all producers are migrated.
- Add runtime diagnostic: project first visible mesh center to clip/NDC and log values (to prove/deny clip rejection).

Exit criteria:
- NDC values for visible mesh centers are sane (w > 0, x/y roughly in range when in view, z in backend expected range).
- No pass requires ad-hoc projection post-fix to be visible.

### Phase 2: Depth/Cull Determinism

Goal: draw visibility is not dependent on accidental state carry-over.

Tasks:
- Ensure effective depth/cull state is explicitly set at pass entry and before draws where policy differs.
- Keep dynamic state path gated by feature support and log active mode in debug builds.
- Normalize skybox depth policy (`LessEqual`) and mesh depth policy (`Less`) as explicit pass policy.

Exit criteria:
- Forward opaque/skybox visibility remains stable across scene changes and frame transitions.
- State logs match expected per-pass policy.

### Phase 3: Stub Elimination / Feature Gating

Goal: no critical debug/profiling path depends on stubs.

Tasks:
- Implement Vulkan query path (`QueryPoolVK`, begin/end query, result read) OR fully gate and label unsupported workflows.
- Keep one-time explicit logs when unsupported features are requested.

Exit criteria:
- Occlusion/sample-pass diagnostics either produce real values or are clearly disabled by capability.

### Phase 4: Regression Safety Net

Goal: prevent reintroduction of invisible-mesh regressions.

Tasks:
- Add golden scenes and backend checks:
  - one opaque mesh,
  - two opaque meshes,
  - skybox-only,
  - opaque + skybox + post.
- Add counters/assert-like checks in debug builds for:
  - visible > 0 with draws == 0,
  - persistent invalid descriptor/pipeline cases,
  - impossible clip-space values.

Exit criteria:
- Automated/local validation catches regressions before manual QA.

## Implementation Tracking

### Completed

- Forward submission diagnostics (`ForwardOpaqueSubmit` counters).
- Vulkan extended dynamic state feature detection log.
- Forward Vulkan state log (`ForwardOpaqueStateVK`).
- Skybox visibility baseline fixes and debug-clear cleanup.

### In Progress

- Phase 1 clip-space contract verification (NDC logging).

### Pending

- Camera-source canonical Vulkan projection migration.
- Renderer projection-adjustment retirement.
- Vulkan query path implementation or strict capability gating.
- Golden-scene regression checks.

## Immediate Next Actions

1. Use NDC logging to confirm whether current invisible meshes are clip/depth rejected.
2. If confirmed, migrate camera projection generation to canonical Vulkan convention at source.
3. Remove renderer-side projection fix once migration completes.
4. Implement/gate Vulkan query diagnostics to close observability gaps.

