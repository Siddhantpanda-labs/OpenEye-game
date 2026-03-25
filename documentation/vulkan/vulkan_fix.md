# Vulkan Refactor Master Plan (Unity/Unreal-Level Robustness)

This document is the single execution plan for Vulkan renderer hardening in OpenEye.
Goal is not "works on my machine", but deterministic, debuggable, cross-pass correctness with explicit contracts.

## North-Star Architecture

- Single source of truth for render contracts:
  - camera policy by backend
  - descriptor-space ABI
  - push-constant ABI
  - depth policy and clear/compare semantics
- No string-based ABI routing on critical paths.
- Passes own typed data contracts (CPU structs mirror shader layouts).
- Validation clean in Vulkan layers for all shipping passes.
- RenderDoc pass-by-pass invariants are documented and reproducible.

## Scope

- In scope:
  - Typed push-constant migration
  - Descriptor set ownership and update lifetime correctness
  - Dynamic/static state policy cleanup (depth/cull/blend)
  - Camera/depth/frustum convention stabilization
  - Regression harness and golden-scene checks
- Out of scope (separate projects):
  - New renderer features (RTGI/path tracing/etc.)
  - Major material system redesign unrelated to ABI correctness

## Phase 0 - Baseline and Guardrails (Immediate)

Objective: lock current behavior and prevent regressions while refactoring.

- Freeze baseline scene set:
  - skybox-only scene
  - single opaque mesh scene
  - mixed opaque + transparent + outline scene
  - shadowed scene (directional + point/spot)
- Enable strict runtime checks:
  - assert on invalid pipeline layout kind usage
  - assert push constant writes stay in declared ranges
  - log-once warnings for legacy uniform string path use
- Add "known-good" evidence per scene:
  - screenshot
  - frame logs (visibility counts + draw counts)

Acceptance criteria:

- Baseline scenes render correctly in Vulkan and OpenGL.
- No new validation errors introduced by guardrails.

## Phase 1 - Typed Push-Constant ABI (Highest Priority)

Objective: eliminate fragile name-based Vulkan push routing.

### 1.1 Define canonical push structs (CPU side)

Create one typed struct per pass contract, e.g.:

- `ForwardVsPush`
- `ForwardFsPush`
- `ShadowPush`
- `EntityIdPush`
- `IBLPush`
- `OutlinePostPush`

Rules:

- Struct field order and alignment explicitly match shader block layout.
- `static_assert(sizeof(...))` for expected ranges and max push size.
- One authoritative header for these contracts (engine-side, not app-side).

### 1.2 Add typed command-list API

Add typed entry points for graphics push updates, for example:

- `SetForwardVsPush(...)`
- `SetForwardFsPush(...)`
- `SetShadowPush(...)`
- `SetEntityIdPush(...)`

or a generic templated route constrained by `PipelineLayoutKind`.

Rules:

- No string parsing in Vulkan push path.
- Stage mask is explicit per push type.
- Range/offset validation occurs centrally.

### 1.3 Migrate passes in safe order

1. Forward (already proven problematic)
2. Shadow
3. Entity ID
4. IBL
5. Outline/Post and remaining push-only paths

### 1.4 Deprecate legacy path

- Keep legacy `SetUniform*("name", ...)` temporarily with warning logs.
- Remove legacy Vulkan string path after all passes migrated.

Acceptance criteria:

- No Vulkan pass depends on uniform-name string matching for push data.
- Forward/Shadow/EntityID/IBL render equivalently before/after migration.
- Validation and RenderDoc show correct push values for each pass.

## Phase 2 - Descriptor ABI and Lifetime Hardening

Objective: formalize descriptor contracts and eliminate update/bind hazards.

- Enforce descriptor-space contracts per `PipelineLayoutKind`.
- Keep per-frame/per-draw ownership explicit (no in-flight rewrites).
- Validate all descriptor sets are complete before bind.
- Add debug labels for set binds by pass and set index.
- Normalize transition policy around passes that sample prior outputs.

Acceptance criteria:

- Zero descriptor update-while-in-flight errors.
- Zero image-layout mismatches in post/outline/forward chains.

## Phase 3 - Dynamic State and Depth/Cull Policy Convergence

Objective: remove hidden state divergence and make depth/cull deterministic.

- Decide policy per backend:
  - dynamic depth/cull state vs pipeline-baked state
  - if dynamic, always reapply at pipeline bind
- Formal depth policy table:
  - clear depth values
  - compare op for each pass
  - reverse-Z decision (current: off unless explicitly enabled)
- Skybox and fullscreen-pass depth semantics documented and fixed.

Acceptance criteria:

- No pass relies on stale dynamic state.
- Depth behavior matches policy table in both mono and stereo paths.

## Phase 4 - Camera/Frustum Convention Stability

Objective: keep camera math centralized and culling convention-safe.

- Keep backend camera policy (`depth range`, `clip-Y`, `reverse-Z`) authoritative.
- Ensure frustum culling uses raw camera/frustum source compatible with extraction math.
- Ban ad-hoc projection tweaks outside camera policy helpers.

Acceptance criteria:

- Gizmo/projected overlays and rendered geometry remain aligned.
- CPU culling matches actual visibility across representative camera poses.

## Phase 5 - Verification Harness (Golden Scenes)

Objective: make regression detection fast and objective.

- Define golden checks per scene:
  - visible mesh count
  - draw count per pass
  - non-empty color/depth targets
  - optional image diff thresholds
- Add CI/automation hook to run Vulkan golden checks.
- Store expected metrics snapshot with versioned update process.

Acceptance criteria:

- Refactors can be validated by running golden harness, not manual guesswork.
- Regressions are caught with clear pass-level failure signal.

## Engineering Standards (Unity/Unreal-Level Expectations)

- Explicit contracts over implicit conventions.
- Strong typing over string routing for shader ABI.
- One owner per system contract:
  - camera policy owner
  - push ABI owner
  - descriptor ABI owner
  - depth policy owner
- Every migration step ships with:
  - invariants/assertions
  - validation cleanliness
  - before/after evidence

## Risks and Mitigation

- Risk: ABI mismatch between CPU struct and shader layout.
  - Mitigation: static asserts, centralized layout docs, RenderDoc spot checks.
- Risk: mixed legacy + typed paths causing temporary drift.
  - Mitigation: warning-on-legacy, phased retirement deadline.
- Risk: hidden dependence on dynamic state.
  - Mitigation: state replay at bind + pass policy tests.

## Execution Order (Strict)

1. Phase 0 (baseline + guardrails)
2. Phase 1 (typed push ABI)
3. Phase 3 (depth/cull state policy)
4. Phase 2 (descriptor cleanup finalization)
5. Phase 4 (camera/frustum hardening pass)
6. Phase 5 (golden-scene automation)

Note: Phase 2 and Phase 3 can partially overlap, but typed push ABI (Phase 1) is the hard gate first.

---

## Progress Tracker (Update This Section Every Work Session)

Legend: `[ ]` not started, `[-]` in progress, `[x]` done, `[!]` blocked

### Global Status

- Current phase: `Phase 1 - Typed Push-Constant ABI`
- Overall completion: `~50%` (estimate)
- Last updated: `2026-03-25`

### Checklist

- [x] Mesh visibility unblock: Vulkan now receives `u_Model` and `u_UseAlbedoTexture` push values.
- [x] Centralized camera policy exists and is backend-typed (no string-only backend identity in core path).
- [x] Forward push-constant ABI conflict fixed: fragment flags moved to dedicated offsets (`128+`) so they no longer overlap vertex `u_Model`.
- [x] Define canonical typed push structs for major passes (Forward, Shadow, EntityID, IBL, OutlinePost) in `PassPushConstants.h`.
- [-] Add typed push APIs in Vulkan command list and route per `PipelineLayoutKind` (legacy compatibility mapping still present for non-migrated callers).
- [-] Migrate Forward pass to typed push API (remove string dependence).
- [x] Migrate Shadow, EntityID, IBL, and OutlinePost Vulkan paths to shared `abi::Write*` helpers (non-Vulkan keeps `Shader::Set*`).
- [ ] Add legacy-path warning and retirement switch.
- [ ] Validate descriptor lifetime/layout correctness across forward->outline->post chain.
- [ ] Finalize dynamic depth/cull policy and enforce at bind.
- [ ] Run golden-scene regression checklist and capture evidence.

### Session Notes

- 2026-03-25: Forward mesh invisibility fixed by wiring missing Vulkan push-constant uniform routes (`u_Model`, `u_UseAlbedoTexture`) and preserving `u_NormalMatrix` as Vulkan no-op parity path. Next target is full typed push ABI migration.
- 2026-03-25: Fixed structural Forward ABI issue: forward/forward+ fragment push constants now use explicit offsets (`u_UseIBL` at 128, `u_UseAlbedoTexture` at 132 where applicable), and Vulkan command routing now writes those offsets for `PipelineLayoutKind::Forward`. This prevents fragment flags from clobbering vertex `u_Model` push data.
- 2026-03-25: Began typed push migration in renderer callsites: Forward and Forward+ now use typed C++ push payloads (`ForwardVsPushData`, `ForwardFsPushData`) and direct `SetPushConstants` on Vulkan, while non-Vulkan paths keep legacy uniform setters for compatibility. Remaining work is to promote these local structs into a shared canonical header and finish removing Vulkan string-based push routing.
- 2026-03-25: Promoted forward push ABI out of `RenderPipeline.cpp` into engine-level `eyelib/render/PassPushConstants.h` (`ForwardVsPush`, `ForwardFsPush`, canonical offsets + writer helper). `RenderPipeline` now consumes this shared contract (`abi::WriteForwardPushConstants`) instead of local ad-hoc structs/functions, keeping orchestration and ABI definition separated.
- 2026-03-25: Extended shared push ABI for `Shadow` and `EntityID` in `PassPushConstants.h` and migrated Vulkan callsites to typed writes. Also fixed `EntityID` push overlap by assigning explicit shader offsets (`model=0`, `view=64`, `projection=128`, `entityId=192`) so vertex and fragment push data no longer alias.
- 2026-03-25: Added IBL and OutlinePost to `PassPushConstants.h` (`WriteIblVertexPushConstants`, `WriteIblPrefilterRoughnessPushConstant`, `WriteOutlinePostPushConstants`). Migrated `IBL.cpp` and `RenderPipeline::RenderOutlineFromIdBuffer` on Vulkan. Shaders: `prefilter.frag` uses `layout(offset=128)` for `u_Roughness`; `outline_post.frag` uses explicit offsets for std430 packing. `CommandListVK::SetUniformFloat` now maps `u_Roughness` to offset 128 for `PipelineLayoutKind::IBL` as a compatibility fallback.
- 2026-03-25: Hardened Vulkan legacy mat4 push routing in `CommandListVK` with layout-aware offsets and model-presence tracking (`m_pushModelSet`) to avoid silent clobbering for legacy debug/push-only shaders (`u_View`/`u_Projection` with and without `u_Model`). Reset at pipeline bind/pool reset.
