# Rendering module ï¿½ implementation order (`Render_steps_todo.md`)

**Purpose:** Single **ordered** checklist for what to build next under `EngineSource/eyelib/rendering/`. Drives the **current scope** (closing **M4** and delivering the first real forward raster path, **M5**). Later phases are parked at the bottom as short anchors; they get expanded into full step tables only when they become the current scope.

**Companion documents**

- `new_render_module.md` ï¿½ target architecture and non?negotiable rules.
- `newrender_roadmap.md` ï¿½ phases, exit gates, **ï¿½1.3 Hardening dimensions** (D1ï¿½D10).
- `Render_check.md` ï¿½ milestone tracker + changelog; update when steps land.
- `EngineSource/eyelib/rendering/README.md` ï¿½ on?disk charter.

**Scope (locked, current):**

- `EngineSource/eyelib/rendering/` (everything above RHI). Vulkan RHI itself is tracked separately in `documentation/vulkan/steps_todo.md`.
- Target outcome of current scope: **M4 closed** + **forward raster end?to?end** in the sandbox (rotating/test mesh visible, correct barriers at compile time, real uploads, real visibility ? draw list).
- Out of scope for *this fileï¿½s active tables*: deferred, shadows, post/temporal, editor?as?feature, RT/GI/neural, `api/rendering`. Listed at the bottom for continuity.

**How to use:** Implement **top to bottom** within each section. Mark `[x]` when a step lands; add a one?line note with date + files touched. When a section is fully `[x]`, **run the hardening stress check for the listed dimensions** and log a short pass/partial/fail note in `Render_check.md` per the template in `newrender_roadmap.md` ï¿½1.3.

**Discipline**

1. **Folders:** Place every new or moved file under the **correct subtree** of `EngineSource/eyelib/rendering/` (and related docs under `documentation/Render_Module/`) per the module charter and roadmapï¿½no ad-hoc locations, no mixing unrelated concerns across top-level areas.
2. **Files & separation of concerns:** Each translation unit should own **one coherent role** (e.g. one pass type, one resource family, one subsystem boundary). Do not scatter one concern across many loosely related files, and do not collapse unrelated logic into a single oversized file; split when responsibilities diverge.
3. **Naming:** Use **precise, professional identifiers** (types, files, passes, blackboard slots, constants)ï¿½specific enough that intent and ownership are clear from the name alone; avoid vague, overloaded, or ï¿½placeholderï¿½ shorthand.

**Build & quality discipline:** After changes under `EngineSource/eyelib/rendering/`, always verify with a **Debug build of `EyelibRendering`** and a **sandbox run of `OpenEyeRenderingSandbox`**. Treat each step as **architecturally correct and complete for its slice**, not ï¿½patched until it compilesï¿½.

```
cd D:\Project\World3d\OpenEye; cmake --build build --target EyelibRendering
cd D:\Project\World3d\OpenEye; cmake --build build --target OpenEyeRenderingSandbox
```

---

## Tracking matrix (current scope)


| ID    | Section / Milestone                                                  | Status                                              | Hardening checks after closure                   |
| ----- | -------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------ |
| M0    | Layout scaffold                                                      | [x] Done                                            | ï¿½                                                |
| M1    | Core orchestration spine                                             | [x] Done                                            | ï¿½                                                |
| M2    | Upload / resources / blackboard / sandbox                            | [x] Done                                            | ï¿½                                                |
| M3    | Scene hook / writer registry / compile hook                          | [x] Done                                            | ï¿½                                                |
| M4.1  | Raster feature bootstrap                                             | [x] Done                                            | ï¿½                                                |
| M4.2  | Fullscreen HDR compose (bootstrap)                                   | [x] Done                                            | ï¿½                                                |
| **A** | **M4 closeout** (uploads, visibility, compile?time barriers, naming) | [x] Done (A.10 logged)                              | D1, D2, D4, D5, D6 (see `Render_check.md` ï¿½A.10) |
| **B** | **M5 forward raster E2E** (first real feature)                       | [x] Done (B.1ï¿½B.4, B.6ï¿½B.11; B.5 optional deferred) | D1, D3, D7, D9, D10                              |


Baseline already landed (do not re?implement): see `Render_check.md` ï¿½Files touched (M2/M3/M4)ï¿½ + `EngineSource/eyelib/rendering/README.md`. Vulkan RHI baseline is tracked in `documentation/vulkan/steps_todo.md`.

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

- **`EngineSource/eyelib/rendering/lighting/` (light data + culling system layer)**
  - Start from existing scaffolds: `SceneLighting`, `LightingSystem`, `LightCuller`, `ClusteredLightGrid`, `TiledLightGrid`.
  - Own: CPU-side light model (directional/point/spot records), light-list/culling policy, and shared light-data assembly for deferred consumption.
  - Keep this layer pass-agnostic: no framegraph scheduling or pass execution code here.

- **`EngineSource/eyelib/rendering/features/shadows/` (feature orchestration only)**
  - Use existing `ShadowFeature.h/.cpp` as the entry feature for P7 scheduling.
  - Own: feature lifecycle (`OnRegister`, `OnUpload`, `OnSchedulePasses`, `OnShutdown`), blackboard slot reservations/publication, shader permutation registration/warm-up.
  - Do **not** place heavy shadow math/policy here.

- **`EngineSource/eyelib/rendering/lighting/shadows/` (shadow policy + data builders)**
  - Extend existing scaffolds: `IShadowStrategy`, `CascadeShadowMap`, `ShadowAtlas`, `VirtualShadowMap`, `RayTracedShadows`.
  - Own: cascade split/stabilization math, atlas allocation policy, CPU-side shadow parameter assembly, quality-tier policy.
  - Keep this layer pass-agnostic (no direct framegraph scheduling logic).

- **`EngineSource/eyelib/rendering/features/raster/` (lighting consumption path)**
  - Keep deferred lighting + shadow sampling integration in raster/deferred consumption code (existing deferred pass/shader contracts).
  - Own only shading-side consumption and bindings; do not move light-model ownership or shadow strategy/policy logic into raster.

- **`EngineSource/eyelib/rendering/framegraph/passes/`**
  - Add new shadow pass files here **only** if they are truly cross-feature/infrastructure-generic.
  - If pass is specific to P7 shadow feature behavior, keep it under `features/shadows`.

- **`EngineSource/assets/shaders/rendering/vulkan/...`**
  - Add/modify P7 shaders under the rendering Vulkan shader tree (follow existing logical path conventions used by `ShaderLibrary` asset paths).
  - Keep shadow map generation and deferred sampling shaders separate (generation vs consumption).

- **Contracts that must be extended before shadow pass wiring**
  - `core/DirectionalLightUniforms.h` + new light buffer contracts: evolve from single directional UBO to P7 light-data contracts (directional/point/spot + shadow params) in upload-phase owned buffers.
  - `framegraph/BlackboardSlot.h` and `framegraph/FrameGraphResourceNames.h`: add typed shadow resources/owners first.
  - `rhi` set/layout contracts: reuse existing shadow-capable pipeline layout/resource-set scaffolding; do not invent parallel ad-hoc set contracts.

Expected new files (only where needed):

- `features/shadows/`:
  - `DirectionalShadowPass.h/.cpp` (required for P7.2/P7.3)
  - `SpotShadowPass.h/.cpp` (P7.4)
  - point-shadow pass files only when P7.5 shadowed point path is enabled
- `lighting/`:
  - add focused light data/culling files only if existing `SceneLighting` / `LightingSystem` / `LightCuller` scaffolds become multi-role or oversized
- `lighting/shadows/`:
  - add focused helper/source files only if existing scaffold files would become multi-role or oversized
- avoid creating new top-level rendering subfolders for P7 unless a clear cross-feature ownership boundary appears

| Step | Status | Implementation order (whole system) |
| ---- | ------ | ---- |
| P7.1 | [x] | **Freeze shadow/light contracts.** Landed typed shadow slots + names (`DirectionalShadowMapArray`, `SpotShadowMapAtlas`, `PointShadowCubemapArray`, `ShadowParamsBuffer`) in blackboard/resource-name constants, plus `features/shadows/ShadowFeature` as an `IRenderFeature` scaffold that reserves/releases these slots under `writer_names::kShadowFeature` (2026-05-07). |
| P7.2 | [x] | **Directional shadow foundation.** Completed (2026-05-07): `DirectionalShadowPass` now renders real opaque/alpha-test casters into 3-layer depth-array shadow map, `ShadowFeature` computes scene-camera-derived directional cascade light-space matrices and uploads `ShadowParamsBuffer`, and deferred lighting consumes/binds/samples directional shadow resources (`RasterDeferredLightingData` bindings 8/9, shader PCF sampling path). |
| P7.3 | [x] | **CSM stabilization + quality.** Completed (2026-05-08): cascade texel-snapping stabilization (`ShadowFeature` snap to frustum center), slope-normal bias tuning, Z-range tightened to `radius*4` (reduces world-space bias dead zone to ~5cm for cascade 0), cascade transition blending with `smoothstep`, and per-cascade light-frustum caster culling. Files: `ShadowFeature.cpp`, `deferred_lighting_analytic.frag`. |
| P7.3a | [x] | **Hardware slope-scaled depth bias.** Completed (2026-05-08): added `SetDepthBias(constantFactor, slopeFactor)` to `ICommandList` (RHI.h) with default no-op body; implemented via `vkCmdSetDepthBias` in `CommandListVK`; added `VK_DYNAMIC_STATE_DEPTH_BIAS` to all graphics pipelines in `PipelineVK.cpp`; added `rasterStateOverride` to `ShaderGraphicsPipelineCreateInfo` (RHITypes.h) and wired through `CreateGraphicsPipelineFromShaderAssetVK`; shadow pipeline sets `depthBiasEnable=true`; `DirectionalShadowPass::Execute` calls `SetDepthBias(1.0f, 2.0f)`; lighting shader NDC depth bias removed (hardware now handles it), only a receiver-side normal slope offset remains. |
| P7.3b | [x] | **Per-cascade soft penumbra (PCF extent tuning).** Completed (2026-05-09): Expose a per-cascade `filterExtent[3]` in `ShadowParamsStd140` (replaces single `pcfKernel` scalar). Cascade 0 = 1 texel (sharp contact), cascade 1 = 2 texels, cascade 2 = 3 texels. Deferred sampling shader reads `uShadow.uFilterExtent[cascadeIndex]` instead of a flat radius. Cap max kernel at 5x5 (radius 2) to bound cost at default quality tier (tied to P7.6 quality settings). |
| P7.3c | [x] | **Directional shadow method finalization (depth+PCF).** Completed (2026-05-09): reverted directional from experimental VSM back to production depth-only CSM path (`CreateDepthOnlyRenderTargetArray` + `directional_depth` pipeline), switched deferred directional sampling to depth-only PCF (`SampleDirectionalShadowPCF`), and aligned barrier/resource state for directional shadow sampling to `DepthRead`. This removed directional VSM-specific lit-side striping/swim artifacts while keeping cascade stabilization and blend logic. |
| P7.4 | [x] | **Spot light shadows.** Completed (2026-05-09): Added `SpotShadowPass` (layered Depth24 shadow array, one layer per casting spot light). `SceneLighting` extended with `SpotLightSceneState` (position, range, inner/outer cone, color, castShadows). `ShadowFeature::OnUpload` builds spot-light projection matrices, uploads `SpotLightArrayStd140` UBO, uses compact shadow-layer indexing with dynamic spot-atlas layer-count reallocation, and renders spot shadows with a dedicated depth-only pipeline. `ShadowFeature::OnSchedulePasses` publishes `SpotShadowMapAtlas` + `SpotLightUniformBuffer` and queues `SpotShadowPass`. Deferred shader (binding 10=UBO, 11=shadow array) accumulates per-spot PBR radiance with depth-only PCF (no VSM for spot). `BlackboardSlot::SpotLightUniformBuffer` added; `RasterDeferredLightingData` layout extended to 12 bindings. |
| P7.5 | [x] | **Point light integration.** Completed (2026-05-09): `SceneLighting` owns `PointLightSceneState` (cap 8) with `ClearPointLights` / `AddPointLight` / `GetPointLights`. `PointLightUniforms.h` defines std140 UBO with six face view-projs per light. `ShadowFeature::OnUpload` builds 90° cubemap face matrices (Vulkan-adjusted proj), allocates depth2DArray (`512²`, `6 ×` up to **4** shadow-casting point lights), uploads UBO, reuses depth-only `directional_depth` pipeline. New `PointShadowPass` (feature-owned) renders all faces. Blackboard: `PointLightUniformBuffer` + `PointShadowCubemapArray` (dummy 1-layer depth when no point shadow RT). `RasterDeferredLightingData` extended to bindings **12–13**; `deferred_lighting_analytic.frag` adds inverse-square + smooth range falloff, face selection + depth PCF sampling. Sandbox: warm point light with shadow beside existing spot. |
| P7.6 | [ ] | **Shadow filtering/perf tiers.** Expose low/medium/high shadow settings (cascade count/resolution/filter kernel) with sane defaults. Low = PCF 1-tap + no VSM. Medium = PCF 3Ã—3 + per-cascade extent. High = VSM + optional blur pass. |
| P7.7 | [ ] | **Screen-space contact shadows.** Add a fullscreen compute pass (after GBuffer, before deferred lighting) that ray-marches each pixel toward the light direction in view-space depth, catching sub-texel contact geometry (e.g. sphere-on-plane base) that CSM at 2048px cannot resolve. Result feeds a 1-bit `contactShadow` mask composited with the CSM shadow term in the deferred lighting shader. Bounded by a max ray step count (16â€“32 taps) for cost control. |
| P7.8 | [ ] | **Validation matrix.** Run sandbox tests for near-edge quality, cascade transitions, camera motion stability, multi-light stress, and mode switching. |
| P7.9 | [ ] | **Hardening closeout.** Record D1/D2/D3/D5/D6/D7/D10 checks in `render_check.md`, then mark P7 complete. |

---

## Section C ï¿½ Parking lot (next?scope anchors, **not** active)

Keep as pointers only; expand into full tables when promoted to current scope.

- **P6:** complete for current scope (see P6.1-P6.12 table above; hardening closeout logged in `render_check.md`).
- **P7 ï¿½ Shadows + light grids.** `lighting/shadows/` (CSM strategy); Forward+ clustered light grid.
- **P8 ï¿½ Post + temporal in the graph.** Tone map, bloom, exposure; `temporal/` with history ownership and camera?cut invalidation.
- **P9 ï¿½ Editor as a feature.** `features/editor/` registers like any other feature; zero cost when not registered.
- **P10 ï¿½ Optional tracks.** RT (`features/raytracing/`), GI (`lighting/gi/`), atmosphere, upscaling/denoising, neural/splats.
- **P11 ï¿½ Public API + migration.** `api/rendering/` PIMPL; migrate sandbox ? editor/player; remove `eyelib/render` when no target links it.
- **Editor entity-ID pass (GPU picking):** wire `features/editor/EntityIDPass` + façade readback so viewport click-to-select works on the new scene-view path (migration M2.5 follow-up; blocked until module implements pass — see capability audit).
- **Editor debug-draw feature:** implement `DebugDrawPass` / colored line submission so scene grid + selection helpers render through `api/rendering` (migration M2.6 follow-up).
- **[RESOLVED 2026-05-12]** Per-scene skybox + IBL probe swap. `api/rendering::RenderScene::skybox` (PIMPL `Skybox` handle: cross-layout, six-face, procedural) drives a new `SkyboxFeature` that (a) appends the sky overlay pass onto `MainHdrRenderTarget` after deferred lighting, and (b) owns the `IblIrradianceCubemap` + `IblPrefilterCubemap` blackboard slots — baking irradiance + GGX-prefiltered specular cubemaps via `IBL::GenerateIrradianceAndPrefilter` whenever the source cubemap pointer changes between frames. `RasterFeature::GetDefaultEnvCubemap()` supplies the fallback for procedural / none. BRDF LUT slot stays with `RasterFeature` (`IblDefaultProbes`, scene-invariant). Editor: `EditorSkyboxCache` keyed by `skyboxAssetId` (procedural / evening alias / arbitrary cross / six-face folder). Player wiring on the new pipeline is a follow-up (`WorldRenderBridge::GetOrLoadSkybox`) — file-load helpers ported, but the player bridge still consumes legacy until M4.x. Wired by `RenderPipeline::Impl::BuildWorldAndFeatures` via `SkyboxFeature::SetDefaultEnvProvider(raster.get())`.
- **Per-draw skinning matrices in `SceneDrawInstance`:** today `eyelib::rendering::SceneDrawInstance` carries no bones field, so per-instance skinned mesh rendering cannot cross the new module's scene contract. Add `const math::Mat4* bones; size_t boneCount;` (frame-allocator backed) + per-draw bone-matrix SSBO upload in `RasterFeature` Forward+Skinned permutation (the shader path already exists for skinned vertex format). Required for player skinned characters; until landed, skinned meshes render at bind pose only on the new path (filed from `editor_player_migration_track.md` M4.0a).
- **Directional light `castShadows` honour at the lighting bus:** `SceneLighting::SetDirectionalLight` does not take a `castShadows` flag today; the new module's `ShadowFeature` always renders the directional CSM if it has any directional. Add a `bool castShadows` to the directional state and gate CSM passes accordingly so the player / editor can disable directional shadows per scene (filed from `editor_player_migration_track.md` M4.0a).
- **Player streaming + readback on `api/rendering` (filed from `editor_player_migration_track.md` M4.5):** today the new module exposes neither (a) a public `RenderTarget` create/blit/readback wrapper sized for VR streaming targets (3 × 960×540 RGBA8 ring), nor (b) async PBO ring (`CreatePixelPackBuffer` / `ReadPixelsToPBO` / `MapPixelPackBuffer{,Raw}` triple-buffered), nor (c) the NV12 packing compute pass (`RunStreamingNV12Pass`: RGBA RT → 1024-pitch NV12 PBO via a CS that writes Y-plane + interleaved UV-plane), nor (d) a sync RGBA8 `ReadOutputPixels` for legacy screenshot calls. Required so `WorldRenderBridge::PrepareStreamingFrame` / `IssueStreamingReadback` / `Get*FromPreviousFrame` lift the M4.5 collapsed-warn pause. Recommended landing order: (1) M4.0c-equivalent — public `RenderTarget::Create(pipeline, w, h, formats, depth, flags)` + `BlitOutputTo(RenderTarget&)` over the new module's RHI texture pool; (2) async PBO via the RHI's existing staging buffer family; (3) NV12 compute as a new `features/streaming/StreamingNV12Pass` (CS-only; opt-in via feature register); (4) sync `ReadOutputPixels` last (only legacy screenshot callers consume it; can stay paused longer). Until landed, standalone player on `kUseNewRenderPipeline` does not stream — the legacy player exe keeps the feature.
- **[RESOLVED 2026-05-11]** Editor multi-pipeline frame boundaries (`BeginPresentFrame` / record-only `Render` / `ComposeSwapchain`) + `RasterFeature::SetPresentationBlitEnabled` for shared-device pipelines.
- **Editor/player legacy `*Descriptor` migration + firewall granularity (filed from `editor_player_migration_track.md` M4.8):** the legacy `eyelib::render::MeshDescriptor` / `TextureDescriptor` / `materials::MaterialDescriptor` are still the AssetRegistry on-disk descriptor types consumed by both `RendererGPUResourceCacheNew` and `BuiltinResourceManagerNew` to seed the new module's `ResourceCache`. They are headers under `eyelib/render/` but they describe asset payload (mesh vertex layout, texture pixel format, material parameter slots), not render runtime. The current `LegacyRenderFirewall.cmake` substring-matches `#include "eyelib/render/` and would over-fire on these descriptor headers, so the per-file opt-in firewall blocks pre-staged in `EngineSource/app/player/CMakeLists.txt` (M4.1 + M4.7 closeout) and `EngineSource/app/editor/CMakeLists.txt` (M3 + M4 closeout) cannot be activated yet without descriptor migration. Two recommended landing options: (a) move the descriptor types to a new neutral namespace (`eyelib::asset::descriptors::*` under `EngineSource/eyelib/asset/`) and rewire `*Descriptor.h` paths — pure path/namespace edit, no logic change; (b) extend `openeye_enforce_no_legacy_render_includes` with an `ALLOW_PATTERNS` list (e.g. `eyelib/render/*Descriptor.h`) so the firewall can scope-down. Option (a) is the architecturally clean answer (descriptors are asset-pipeline data, not render runtime); option (b) is the quick unblock if descriptor migration is parked. Until landed, the editor cutoff is enforced by code review + CMake link discipline (M4.8 dropped `EyelibRenderAPI` PUBLIC from `OpenEyeGameRuntime` and re-added it explicitly to `OpenEyeEditor` so the legacy reach is auditable).
- **Player XR stereo on `api/rendering` (filed from `editor_player_migration_track.md` M4.6):** today `RenderPipeline::RenderStereo(scene, leftCam, rightCam)` is a paused stub (delegates to mono `Render(scene)`), `BlitOutputTo(RenderTarget&)` is a no-op, `BlitStereoToScreen(left, right, ...)` is a no-op, and there is no per-eye `RenderTarget`. Required so `WorldRenderBridge::Render` can drive the XR session loop (BeginFrame → per-eye render + BlitOutputTo → EndFrame with handles) and the stereo-preview path (single-pass two-viewport render + BlitStereoToScreen) on the new module. Recommended landing order: (1) public `RenderTarget` from the streaming entry above (shared dependency); (2) `RasterFeature` stereo permutation (cull once, draw twice with viewport split or layered rendering); (3) `BlitOutputTo` real impl (RHI texture-to-texture copy); (4) `RenderStereo` adapter that runs the stereo permutation + emits two camera contexts to the existing `IRenderSceneView` shape. Until landed, standalone player on `kUseNewRenderPipeline` falls back to mono — the legacy player exe keeps stereo. (Note: editor never rendered XR — only the `panels/GameViewPanel.cpp` debug-camera preview path printed "stereo paused"; this is purely a player feature.)
- **[RESOLVED 2026-05-11]** Shadow-feature spot fallback publication: when no spot lights cast shadows, `SpotShadowMapAtlas` now publishes the shared dummy shadow array (pre-transitioned for shader sampling) instead of an unwritten spot depth target. This removes first-use read-from-undefined layout warnings in deferred lighting during multi-pipeline editor frames.
- **[RESOLVED 2026-05-11]** Feature scheduling order is now explicit (`IRenderFeature::SchedulingOrder`) and registry-stable (ascending order, insertion-stable for ties). `ShadowFeature` runs before `RasterFeature`, so shadow-map producers are scheduled before deferred/forward shadow sampling consumers in multi-pipeline frames.
- **[RESOLVED 2026-05-11]** Offscreen output layout contract for shared-device / window-non-compositor pipelines (`framegraph/passes/PublishOffscreenOutputPass.h`): a `RasterFeature` running with `m_presentationBlitEnabled == false` now appends a finalising barrier-only pass that transitions `MainHdrRenderTarget.color0` to `ResourceState::ShaderResource` at end-of-frame, so a host compositor (e.g. editor ImGui viewport sampling) can bind the output as a sampled image without further synchronisation. Symmetric to the side-effect transition that `BlitMainHdrToDefaultPass` already provided in the swapchain-owner shape; expressed explicitly so the contract is independent of whether the swapchain blit runs.

---

## Cross?cutting rules (repeat to resist drift)

- If it writes a texture or buffer, it is a **graph pass** (doc ï¿½1, D1).
- Features talk **only** via the blackboard; disabled features register **zero** passes (doc ï¿½2/ï¿½3, D2/D3).
- `scene ? core ? framegraph ? scheduler ? features ? ï¿½ ? rhi` ï¿½ no reverse includes (D4).
- Capabilities captured at `OnRegister`; **no** `GetOrCompile` in `Execute` (doc ï¿½5/ï¿½10, D7).
- No `ICommandList`* stored across frames (doc ï¿½9, D8).
- All resource/pass names come from central constants (doc ï¿½11).
- GPU scopes on every pass from the first real feature on (doc ï¿½13, D10).

---

*Update this file whenever a step lands (status + one?line note). Promote Section C items into full tables only when they become the current scope.*
