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

## Section A - M4 closeout - Done

Summary: compile-time barrier planning, typed draw list + visibility, upload-phase UBO wiring, and naming/blackboard discipline are complete.
Hardening: D1/D2/D4/D5/D6 logged in `render_check.md` (A.10).

| Step | Status | Note |
| ---- | ------ | ---- |
| A.1 | [x] | Central pass/resource/writer name constants landed. |
| A.2 | [x] | Blackboard slot expansion + typed publish/get APIs landed. |
| A.3 | [x] | Typed `DrawList` adopted. |
| A.4 | [x] | CPU frustum visibility + sorted draw list integrated. |
| A.5 | [x] | Real PerFrame/PerView UBO upload path integrated. |
| A.6 | [x] | `FrameGraph::Compile` barrier collect/dedupe/merge implemented. |
| A.7 | [x] | Debug barrier validators added and false-positive noise reduced. |
| A.8 | [x] | Upload phase guardrails/hardening landed. |
| A.9 | [x] | Sandbox smoke validated clear/compose/blit path. |
| A.10 | [x] | Hardening check logged in `render_check.md`. |


---

## Section B - M5 forward raster end-to-end - Done

Summary: first complete forward path (clear/forward/blit) landed with permutation management, provider-driven geometry submission, PerObject uploads, and pass-level profiling.
Hardening: D1/D3/D7/D9/D10 logged in `render_check.md`.

| Step | Status | Note |
| ---- | ------ | ---- |
| B.1 | [x] | Permutation declaration landed (`ShaderLibrary`). |
| B.2 | [x] | Pipeline warm-up integrated; no execute-time compilation. |
| B.3 | [x] | `IGeometryProvider` + static mesh provider integrated. |
| B.4 | [x] | PerObject SSBO upload path integrated. |
| B.5 | [ ] | Optional depth prepass (kept deferred). |
| B.6 | [x] | Forward opaque pass integrated. |
| B.7 | [x] | Minimal forward shading path validated. |
| B.8 | [x] | Pass-level profiling/labels integrated. |
| B.9 | [x] | Sandbox scene path verified. |
| B.10 | [x] | Hardening checks logged. |
| B.11 | [x] | M5 closeout completed in docs/checklist. |

---

### Bx - Attachment ops contract - Done

Summary: explicit attachment load/store/clear contracts were added and wired to Vulkan dynamic rendering, with compile-time conflict checks.

| Step | Status | Note |
| ---- | ------ | ---- |
| Bx.1 | [x] | `FrameGraphAttachmentOps` contract type added. |
| Bx.2 | [x] | Pass API hook (`DescribeRenderAttachmentOps`) added. |
| Bx.3 | [x] | Vulkan mapping for explicit attachment ops landed. |
| Bx.4 | [x] | Forward pass migrated to explicit begin-clear. |
| Bx.5 | [x] | Compile-time conflicting attachment-op warnings added. |
| Bx.6 | [x] | Sandbox/docs closeout completed. |

---

## Section P6 - Deferred implementation sequence - Done

Summary: deferred PBR + material system delivered with contract-first layout, blackboard ownership, upload discipline, parity/mode/perf validation, and hardening closeout.

| Step | Status | Note |
| ---- | ------ | ---- |
| P6.1 | [x] | Deferred contracts frozen (`RasterGBuffer` + `RasterDeferredLighting`). |
| P6.2 | [x] | Canonical material schema landed. |
| P6.3 | [x] | Runtime material table ownership landed. |
| P6.4 | [x] | Material-table upload path integrated. |
| P6.5 | [x] | GBuffer material channel path completed. |
| P6.6 | [x] | Deferred lighting upgraded to GGX PBR. |
| P6.7 | [x] | IBL ownership/contracts integrated with robust LUT fallback. |
| P6.8 | [x] | Material texture/fallback parity validated. |
| P6.9 | [x] | Forward/deferred mode-switch correctness validated. |
| P6.10 | [x] | Perf/stability pass reduced redundant material upload churn. |
| P6.11 | [x] | Expanded sandbox validation matrix completed (10 cases). |
| P6.12 | [x] | Deferred hardening closeout (D1/D2/D3/D5/D6/D7/D10) logged. |

### P6 archive (completed)

- P6.1-P6.12 are complete for the current deferred scope.
- Final contracts/architecture reference: `EngineSource/eyelib/rendering/features/raster/DEFERRED_ARCHITECTURE.md`.
- Hardening closeout reference: `documentation/Render_Module/render_check.md` (`P6.12 - Deferred hardening closeout`).

---

## Section P7 - Lights + shadows implementation sequence (active next)

Goal: deliver professional, stable real-time lighting/shadows (directional/point/spot) with clean ownership and controlled runtime cost (soft edges, no obvious jagged artifacts, stable near-field quality).

Architecture guardrails (mandatory for all P7 work):

- All shadow/light GPU writes are graph-pass owned (D1); no ad-hoc command tail.
- Feature communication is blackboard-only (D2/D3); no cross-feature direct calls.
- Light and shadow constants upload in upload phase only (D5).
- No execute-time compile/create (`GetOrCompile`) in pass execution (D7).
- Keep raster/shadow pass logic feature-owned under `features/raster` and shadow policy under `lighting/shadows`.
- Quality tuning must be tiered and bounded (no unbounded kernel/cascade growth in default path).

Default quality/perf target (initial):

- Directional: 3-cascade CSM, stable cascade snapping, blend band, PCF 2x2 or 3x3.
- Spot: single 2D shadow map + PCF 2x2.
- Point: phased rollout (initially unshadowed or capped hero-light shadow).
- Near cascade quality prioritized; far cascade allowed lower effective detail.

### P7 placement + ownership note (must follow)

Use existing folders as follows to avoid wrong placement or extra files:

- **`EngineSource/eyelib/rendering/features/shadows/` (feature orchestration only)**
  - Use existing `ShadowFeature.h/.cpp` as the entry feature for P7 scheduling.
  - Own: feature lifecycle (`OnRegister`, `OnUpload`, `OnSchedulePasses`, `OnShutdown`), blackboard slot reservations/publication, shader permutation registration/warm-up.
  - Do **not** place heavy shadow math/policy here.

- **`EngineSource/eyelib/rendering/lighting/shadows/` (shadow policy + data builders)**
  - Extend existing scaffolds: `IShadowStrategy`, `CascadeShadowMap`, `ShadowAtlas`, `VirtualShadowMap`, `RayTracedShadows`.
  - Own: cascade split/stabilization math, atlas allocation policy, CPU-side shadow parameter assembly, quality-tier policy.
  - Keep this layer pass-agnostic (no direct framegraph scheduling logic).

- **`EngineSource/eyelib/rendering/features/raster/` (lighting consumption path)**
  - Keep deferred lighting shadow sampling integration in raster/deferred consumption code (existing deferred pass/shader contracts).
  - Own only shading-side consumption and bindings; do not move strategy/policy logic from `lighting/shadows` into raster.

- **`EngineSource/eyelib/rendering/framegraph/passes/`**
  - Add new shadow pass files here **only** if they are truly cross-feature/infrastructure-generic.
  - If pass is specific to P7 shadow feature behavior, keep it under `features/shadows`.

- **`EngineSource/assets/shaders/rendering/vulkan/...`**
  - Add/modify P7 shaders under the rendering Vulkan shader tree (follow existing logical path conventions used by `ShaderLibrary` asset paths).
  - Keep shadow map generation and deferred sampling shaders separate (generation vs consumption).

- **Contracts that must be extended before shadow pass wiring**
  - `framegraph/BlackboardSlot.h` and `framegraph/FrameGraphResourceNames.h`: add typed shadow resources/owners first.
  - `rhi` set/layout contracts: reuse existing shadow-capable pipeline layout/resource-set scaffolding; do not invent parallel ad-hoc set contracts.

Expected new files (only where needed):

- `features/shadows/`:
  - `DirectionalShadowPass.h/.cpp` (required for P7.2/P7.3)
  - `SpotShadowPass.h/.cpp` (P7.4)
  - point-shadow pass files only when P7.5 shadowed point path is enabled
- `lighting/shadows/`:
  - add focused helper/source files only if existing scaffold files would become multi-role or oversized
- avoid creating new top-level rendering subfolders for P7 unless a clear cross-feature ownership boundary appears

| Step | Status | Implementation order (whole system) |
| ---- | ------ | ---- |
| P7.1 | [ ] | **Freeze shadow/light contracts.** Define blackboard slots, set/binding layout contracts, pass order, and fallback rules for missing shadow resources. |
| P7.2 | [ ] | **Directional shadow foundation.** Add directional shadow-map pass, light-space matrix upload, and deferred directional shadow sampling path. |
| P7.3 | [ ] | **CSM stabilization + quality.** Implement 3 cascades, practical split scheme, texel snapping, cascade blending, and tuned depth/normal/slope bias. |
| P7.4 | [ ] | **Spot light shadows.** Add spot-light shadow pass and deferred spot shadow sampling with bounded filter cost. |
| P7.5 | [ ] | **Point light integration.** Add point lights to deferred shading; shadowed point path as phased/quality-tier feature with strict cost controls. |
| P7.6 | [ ] | **Shadow filtering/perf tiers.** Expose low/medium/high shadow settings (cascade count/resolution/filter kernel) with sane defaults. |
| P7.7 | [ ] | **Validation matrix.** Run sandbox tests for near-edge quality, cascade transitions, camera motion stability, multi-light stress, and mode switching. |
| P7.8 | [ ] | **Hardening closeout.** Record D1/D2/D3/D5/D6/D7/D10 checks in `render_check.md`, then mark P7 complete. |

---

## Section C � Parking lot (next?scope anchors, **not** active)

Keep as pointers only; expand into full tables when promoted to current scope.

- **P6:** complete for current scope (see P6.1-P6.12 table above; hardening closeout logged in `render_check.md`).
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