# OpenEye Next-Generation Rendering Module — Roadmap

**Companion documents**

- `[new_render_module.md](new_render_module.md)` — target capabilities, directory layout, and architectural rules (frame graph, blackboard, features, dependency direction).
- Future: `EngineSource/eyelib/rendering/README.md` — on-disk module charter once the tree exists.

**Module location (authoritative)**

- **New code:** `EngineSource/eyelib/rendering/`
- **Legacy code (frozen during migration):** `EngineSource/eyelib/render/`
- **Client applications:** continue to use `api/render` and `eyelib/render` until a deliberate migration phase; they **must not** depend on `eyelib/rendering` until `api/rendering` (or equivalent) is introduced and stable.

## Discipline (layout, ownership, naming)

1. **Folders:** Keep every file in the **right folder** under `EngineSource/eyelib/rendering/` (and companion docs under `documentation/Render_Module/`) per the charter and the directory rules in the companion spec—no ad-hoc roots, no placing code where another subsystem already owns the concern.
2. **Files & separation of concerns:** Prefer **focused translation units**—each file governs **one clear role** (one pass, one resource family, one boundary). Avoid scattering one feature across many incidental files **and** avoid dumping unrelated logic into a single catch-all file; split when responsibilities diverge.
3. **Naming:** Use **correct, precise, professional names** for types, files, passes, blackboard slots, and constants—names should carry intent and lifetime, not shallow or overloaded abbreviations.

This roadmap describes how to reach a **state-of-the-art, industry-grade** rendering stack: clean separation of concerns, explicit dependencies, deterministic frame flow, and extensibility comparable in *structure* to modern engines (explicit frame graph, capability-gated features, data-driven scheduling), without tying the narrative to any legacy file names beyond what the companion doc already maps.

---

## 1. Vision and success criteria

### 1.1 Vision

Deliver a **second-generation** renderer under `eyelib/rendering/` that:

- Treats the **frame graph** as the single authority for GPU writes (including post, temporal, and editor work).
- Uses a **scheduler** plus **render features** with a strict **lifecycle contract** (register → prepare → schedule passes → shutdown).
- Exposes a **read-only scene contract** to GPU consumers and keeps **visibility**, **geometry**, **lighting**, and **post** as separate concerns.
- Scales toward hybrid raster / RT / temporal / neural paths through **pluggable features**, not conditional spaghetti inside one god type.

### 1.2 Success criteria (exit gates)


| Gate                     | Description                                                                                                                                                     |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **G0 — Boundary**        | No translation unit under `rendering/` `#include`s anything under `eyelib/render/`. Enforced by automated grep (CI or presubmit script).                        |
| **G1 — Compile**         | `EyelibRendering` (or agreed target name) links in isolation; a **sandbox** executable links only sandbox + `EyelibRendering` (+ core/math as needed).          |
| **G2 — Frame contract**  | Documented frame phases: `BeginFrame` → **Upload phase** → schedule → compile → execute → `EndFrame`. Upload is never a no-op by design once scene data exists. |
| **G3 — Graph integrity** | Every GPU write goes through the frame graph; post and editor paths are graph passes, not ad-hoc tail recording (per companion doc §1).                         |
| **G4 — Parity slice**    | At least one full-screen or mesh path, shadows, and HDR/post chain proven on target RHI (Vulkan first recommended).                                             |
| **G5 — API**             | Stable `api/rendering` surface (PIMPL as in existing OpenEye API norms) for apps; legacy `api/render` unchanged until cutover.                                  |
| **G6 — Cutover**         | Editor/player switch to `api/rendering`; `eyelib/render` removed when no target links it.                                                                       |


---

## 1.3 Hardening dimensions (architectural discipline)

These are the **axes the architecture is most likely to rot on over time**. They are not phases and not features — they are **stress tests** we re-run after each basic piece lands, so that the guarantees in §1.1 and the rules in §2 and the companion doc actually hold in code, not only on paper.

**Cadence.** After every milestone in `Render_check.md` that adds real behavior (e.g. a new feature, a new pass kind, upload path changes, a new blackboard resource), walk through the list below in order. For each dimension, produce a short **pass / partial / fail** note in `Render_check.md` with the specific files or passes inspected. Do **not** start a new milestone with any dimension in **fail** state.


| #   | Dimension                         | What we check (short)                                                                                                                                                  | Primary failure mode                                                                              |
| --- | --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| D1  | **Frame graph integrity**         | Every GPU write is a declared pass; no direct recording outside `FrameGraph::Execute`; post / editor / debug are passes.                                               | “Tail recording” after the graph; bypasses that skip barriers and aliasing.                       |
| D2  | **Blackboard hygiene**            | Named resources only; **single writer** per resource; names come from a central constants header; reads are typed, not string-soup.                                    | Blackboard turns into a global grab bag with hidden dependencies.                                 |
| D3  | **Feature isolation**             | Features talk **only** via blackboard; no cross-feature includes, no direct calls; disabled features register **zero** passes.                                         | Features grow pointers to each other; can’t be removed or reordered.                              |
| D4  | **Dependency direction**          | Layer DAG (`scene → core → framegraph → scheduler → features → … → rhi`) holds in **includes and CMake**; no reverse edges.                                            | Cycles that force whole-module rebuilds and make swaps (RHI, lighting) impossible.                |
| D5  | **Upload phase reality**          | All data required for the first draw is uploaded in the **upload phase**, never lazily inside pass execute; barriers around upload → read are explicit.                | Silent data races / stale frames; worsens under streaming (Nanite-style, VSM, neural weights).    |
| D6  | **Barriers & scheduling**         | Barriers are computed at graph **compile** (global view), not only ad-hoc per-pass; aliasing / async-compute ordering is visible to the graph.                         | Over-barriering (perf) or under-barriering (correctness) that only shows up on specific GPUs.     |
| D7  | **Capabilities & permutations**   | `RenderCapabilities` is read at `**OnRegister`**; no hot-path `if (supports X)`; shader permutations declared at register, **no `GetOrCompile` in execute**.           | Runtime stalls, GPU-specific crashes, unbounded permutation churn.                                |
| D8  | **Command-list & queue lifetime** | `ICommandList*` never stored across frames; queue choice is declared at schedule time, never by hand-submits inside features.                                          | Async-compute correctness bugs; use-after-free on queue/frame boundaries.                         |
| D9  | **Temporal ownership**            | History / ping-pong / jitter / camera-cut invalidation owned by **one** temporal system; features request handles, never allocate their own “previous frame” textures. | Every denoiser/upscaler invents its own history; impossible to reason about correctness globally. |
| D10 | **Observability**                 | GPU scopes on every pass; RenderDoc hook wired; per-pass timings exported. Debt gets filed against captures, not hunches.                                              | Optimizing the wrong thing; regressions land silently.                                            |


**How a check looks (template):**

- **Dimension:** D2 (Blackboard hygiene)
- **Scope:** milestone M4 (upload / visibility / barrier work)
- **Inspected:** `framegraph/FrameGraphBlackboard.h`, `BlackboardWriterRegistry.h`, all `features/`** writers.
- **Result:** pass / partial / fail
- **Notes / follow-ups:** 1–3 bullets, file-level.

Keep it lightweight. The point is to catch rot **early and cheaply**, not to produce paperwork.

---

## 2. Non-negotiable rules

### 2.1 Dependency firewall

- `**eyelib/rendering/` must never include `eyelib/render/`.**  
Implementation knowledge from the legacy module may inform **design and testing**, but **must not** appear as includes, links, or copy-paste of legacy-specific types tied to `RenderPipeline` or old pass systems.

### 2.2 Allowed bootstrap: copy, then own

For **mechanically independent** subtrees, the project may **copy** sources into `rendering/` and immediately treat them as **owned** by the new module:

- `**rhi/`** (Vulkan/OpenGL/Diligent as applicable) — copy, re-root includes to `eyelib/rendering/rhi/...`, use namespace `eyelib::rendering::rhi` (or agreed prefix) to avoid ODR clashes when both libraries link in migration tests.
- **Contract-style headers** and small libraries that depend only on RHI, math, core logging, and standard library (e.g. push constant layouts, texture resource wrappers, `2d/` if self-contained after include fixup).

After copy, **all edits happen only in `rendering/`** until the legacy tree is deleted.

### 2.3 Dependency direction (one-way)

Mirror the companion document’s layering. **Suggested canonical order:**

```text
scene  →  core  →  framegraph  →  scheduler  →  features  →  (visibility, geometry, lighting, materials, shaders)  →  resources  →  rhi
```

**Lower layers never include higher layers.** `rhi` does not include `features`. `framegraph` does not include `scheduler`. CMake `target_link_libraries` should reflect the same DAG.

### 2.4 Naming and namespaces

- **Include root:** `eyelib/rendering/...`
- **C++ namespace:** e.g. `eyelib::rendering` for module surface; nested namespaces per area (`rhi`, `scene`, `framegraph`, …).
- **Public API:** `eyelib::api::…` under `api/rendering/` when exposed to apps (later phase).

Consistent prefixes avoid collisions with legacy `eyelib::render` during dual-library phases.

---

## 3. Target directory structure (under `eyelib/rendering/`)

The on-disk layout follows `[new_render_module.md](new_render_module.md)` with the **root renamed** from `eyelib/render/` to `eyelib/rendering/`. Folders are **roles**, not grab bags.

```text
eyelib/rendering/
├── rhi/                    # Device, queues, command lists, barriers, bindless — copied then owned here
├── scene/                  # Read-only scene contract; no GPU types unless unavoidable thin DTOs
├── core/                   # RenderWorld, FrameContext, RenderContext, RenderResources, capabilities, settings, profiler hooks
├── framegraph/             # Declarative passes, resources, compile, execute; no feature business logic
├── scheduler/              # FrameScheduler, feature registry, feature set, scheduler context
├── visibility/             # Culling, draw lists, occlusion — outputs consumed by geometry/features
├── geometry/               # IGeometryProvider and mesh/splat/SDF providers — no lighting knowledge
├── materials/
├── shaders/                # Shader cache, permutations, reflection, pipeline cache
├── lighting/               # Scene lights, grids, shadows/, gi/, reflections/, ibl/
├── features/               # raster/, shadows/, atmosphere/, raytracing/, editor/, …
├── temporal/               # History, TAA, motion vectors, upscalers, denoisers
├── post/                   # Post stack, tone map, bloom, etc. — graph passes only
├── resources/              # Mesh, textures, loaders, pools
├── 2d/                     # Sprites, text, primitives (after include isolation)
├── debug/                  # Stats, RenderDoc hooks, optional dev overlays
└── README.md               # Module charter + link to this roadmap
```

**Rule reminders from the companion doc:** `scene/` knows nothing about features; `scheduler/` orchestrates and never draws; `framegraph/` is pure machinery; inter-feature sharing goes through a **typed blackboard** with single-writer semantics.

---

## 4. Phased delivery plan

Phases are **sequential** where noted; some later sub-phases can run in parallel only after interfaces are proven (companion doc §14).

### Phase 0 — Foundation and governance (week-scale)

**Objectives**

- Create `eyelib/rendering/` tree skeleton (empty or stub headers only where needed).
- Add CMake target `EyelibRendering`; add `OpenEyeRenderingSandbox` (or `app/rendering_sandbox`) that links **only** what is required for the new module.
- Add **grep/CI rule**: fail if `eyelib/render/` appears in any include under `eyelib/rendering/`.
- Add `eyelib/rendering/README.md`: boundary statement, namespace, link to this roadmap and `new_render_module.md`.

**Exit:** Sandbox builds and runs (clear screen or no-op frame is acceptable).

---

### Phase 1 — RHI bootstrap (copy-own)

**Objectives**

- Copy `rhi/` from legacy into `rendering/rhi/` (per §2.2); fix all internal includes and namespaces.
- Prove device creation, swapchain or equivalent presentation path, and one command buffer begin/submit/end on the sandbox.

**Risks**

- Duplicate global state (GL loaders, validation layers): ensure only one RHI stack initializes per process in sandbox; dual-link tests need symbol isolation.

**Exit:** G0 + G1 satisfied for RHI subset; documented list of copied files vs. intentionally omitted backends (e.g. Vulkan-only first).

---

### Phase 2 — Contracts and leaf resources

**Objectives**

- Land `scene/` types: camera, draw lists, `IRenderSceneView`-style interface (names may differ; keep semantic contract).
- Land `core/`: `RenderCapabilities`, `RenderSettings`, `FrameContext`, `RenderContext`, `RenderResources` shell (allocate swapchain-sized RTs only).
- Copy **independent** contract/resource files (push constants layout, texture types, `2d/` after dependency audit) with includes rewired under `rendering/`.

**Exit:** No circular headers; `scene` builds without including `features` or `scheduler`.

---

### Phase 3 — Frame graph and scheduler (no “real” shading yet)

**Objectives**

- Implement or port **frame graph** into `framegraph/` (pure: passes, resources, barriers, compile, execute).
- Implement `scheduler/`: `FrameScheduler`, `IRenderFeature` lifecycle as in companion doc (`OnRegister`, `OnFramePrepare`, `OnSchedulePasses`, `OnShutdown`).
- Introduce `FrameGraphBlackboard` with **named resources** and single-writer validation at schedule time.
- `**RenderWorld`** (or equivalent) owns device, `RenderResources`, scheduler, and graph; **no** god-object pass implementations yet.

**Exit:** One dummy feature inserts a clear pass and a fullscreen pass through the graph; barriers validated in RenderDoc.

---

### Phase 4 — Upload phase and resource flow

**Objectives**

- Real `**UploadPhase`** (companion §8): staging transforms, material constants, light buffers — completed before graph execute.
- Centralize transient resource allocation (`RenderResourcePool` or equivalent) where the doc suggests.

**Exit:** Deterministic ordering documented; no uploads inside pass execute callbacks for data that must be visible to the first draw.

---

### Phase 5 — First real feature: raster, forward-only

**Objectives**

- Implement `**features/raster/`** with **forward path only** first (companion §14): depth + HDR, one directional light optional, minimal PBR.
- `visibility/` produces draw lists; `geometry/` submits mesh draws; **lighting** supplies constants via blackboard, not direct feature-to-feature calls.

**Exit:** Rotating mesh or equivalent in sandbox; GPU capture shows clean pass order.

---

### Phase 6 — Raster: deferred and mode switching

**Objectives**

- Add G-buffer passes, deferred lighting pass, and **mode switching** (forward vs deferred) via feature configuration or sub-strategy, still within `RasterFeature`.
- Merge or align shader asset paths: either duplicate shaders under `EngineSource/eyelib/rendering/assets/shaders/...` or read existing disk assets as **data only** (no legacy includes). Document which approach is authoritative.

**Exit:** Visual parity with a defined test scene; performance baseline captured (profiler scopes on every pass).

---

### Phase 7 — Shadows and lighting depth

**Objectives**

- `lighting/shadows/`: CSM strategy behind `IShadowStrategy`; shadow feature registers graph passes.
- Light grids for forward+ (clustered) as separate milestone inside lighting.

**Exit:** Blackboard-driven shadow atlas / cascade data; no shadow sampling hard-wired inside `geometry/`.

---

### Phase 8 — Post and temporal (graph-native)

**Objectives**

- Move tone mapping, bloom, exposure into `**post/`** as **graph passes** (not post-graph tail execution).
- Introduce `**temporal/`**: motion vectors, history handles, invalidation on camera cuts; TAA or placeholder pass.

**Exit:** Companion rule “post is in the graph” satisfied; history ownership centralized.

---

### Phase 9 — Editor and tooling feature

**Objectives**

- `features/editor/`: entity ID, debug draw, overlays — **registered** like any feature; absent in shipping builds by non-registration, not scattered `#ifdef` (companion §12).

**Exit:** Picking works in sandbox or editor prototype hooked to new API only.

---

### Phase 10 — Advanced features (parallel tracks after Phase 8)

Each track is optional and capability-gated (`RenderCapabilities` at register time, companion §5):


| Track                   | Content                                                               |
| ----------------------- | --------------------------------------------------------------------- |
| **RT**                  | `features/raytracing/`, BLAS/TLAS, hybrid denoised RTAO / reflections |
| **GI**                  | `lighting/gi/` probes, DDGI or agreed MVP                             |
| **Atmosphere**          | `features/atmosphere/`                                                |
| **Upscaling / denoise** | `temporal/upscaling/`, `temporal/denoising/` behind interfaces        |
| **Neural / splats**     | `features/neural/`, `geometry/GaussianSplatGeometry` as stubs first   |


**Exit per track:** feature can register zero passes when disabled; no runtime hot-path capability branching beyond cached flags set at registration.

---

### Phase 11 — Public API and migration

**Objectives**

- Add `api/rendering/` mirroring OpenEye API conventions (PIMPL, stable headers).
- Migrate **one** app (sandbox → player or editor) behind feature flag.
- Deprecate `api/render`; remove `eyelib/render` when no CMake target references it.

**Exit:** G5 + G6.

---

## 5. Engineering standards (throughout)

### 5.1 Frame graph constitution

- If it writes a texture or buffer, it is a **graph pass** (companion §1). Post, temporal resolve, editor overlays included.

### 5.2 Command list lifetime

- Do not store `ICommandList*` across frame boundaries; pass by parameter into execute scopes (companion §9; aligns with modern RHI practice).

### 5.3 Pipeline compilation

- No `GetOrCompile` inside hot pass callbacks; permutations declared at feature registration, warm-up path documented (companion §10).

### 5.4 Resource naming

- Central **string constants** (or strong types) for blackboard and graph resource names — no scattered literals (companion §11).

### 5.5 Profiling and debugging

- GPU scopes on every pass from first real feature onward; RenderDoc capture hook early (companion §13).

### 5.6 Determinism and robustness

- Stable pass order; explicit invalidation for temporal; device-loss reporting hooks in RHI wrapper layer (companion §12).

---

## 6. CMake and linking strategy

- `**EyelibRendering`**: sources only under `EngineSource/eyelib/rendering/`.
- **Sandbox**: links `EyelibRendering`, `EyelibCore` (or minimal deps), **not** `EyelibRender`.
- **Migration build**: optional CMake flag to build both stacks for comparison **only if** namespaces and symbol export prevent clashes; default off.

---

## 7. Shader and asset policy

Choose one explicitly and document in `rendering/README.md`:

1. **Fork shaders** under `eyelib/rendering/assets/...` (recommended for long-term independence), or
2. **Share files on disk** from existing `EngineSource/assets/...` as read-only data, while **all** metadata and layout contracts live under `rendering/shaders/` headers.

Either way, **no** `#include` from `rendering` into legacy C++ headers.

---

## 8. Testing matrix


| Layer       | Unit / integration                                                  |
| ----------- | ------------------------------------------------------------------- |
| RHI         | Device init, resize, swap, fence sync                               |
| Frame graph | Compile barriers, alias correctness on synthetic passes             |
| Scheduler   | Feature order, zero-pass features, blackboard single-writer asserts |
| Raster      | Golden frame hash optional; RenderDoc manual checklist              |
| API         | ABI-smoke tests when `api/rendering` exists                         |


---

## 9. Risk register (short)


| Risk                      | Mitigation                                                                            |
| ------------------------- | ------------------------------------------------------------------------------------- |
| Two RHI copies drift      | Single ownership doc; port fixes explicitly; shorten dual-stack window                |
| Graph over-centralization | Keep `framegraph/` free of game logic; all logic in features                          |
| Namespace / link clashes  | `eyelib::rendering` + separate target; avoid duplicate free functions at global scope |
| Scope creep               | Capability gates; disable entire features when unsupported                            |


---

## 10. Document maintenance

When a phase completes, update:

- This file (checklist and dates).
- `eyelib/rendering/README.md` (current capabilities and known gaps).
- Optionally `documentation/refractor/Render_module_arc.md` only to add a **pointer** to the new module (legacy blueprint remains valid for `eyelib/render` until removal).

---

## 11. Summary checklist (printable)

- P0: Tree + CMake + configure-time include firewall + sandbox (`OpenEyeRenderingSandbox`, `EyelibRendering`)  
- P1: RHI copy-own + present clear  
- P2: Scene + core shells + independent contracts  
- P3: Frame graph + scheduler + blackboard + RenderWorld  
- [x] P4: Upload phase + transient pools  
- [x] P5: Raster forward E2E  
- P6: Raster deferred + modes  
- P7: Shadows + light grids  
- P8: Post + temporal in graph  
- P9: Editor feature  
- P10: RT / GI / atmosphere / upscale (optional tracks)  
- P11: `api/rendering` + migration + remove legacy



---

Here is a complete, industry-level shader stack for the next-gen renderer, grouped by domain. All paths are under `EngineSource/assets/shaders/rendering/vulkan/`. Logical-name only (backend adds `.vert/.frag/.comp/.rgen/...`).

## 0. Shared include-only (`common/`, not loadable)
`scene_types.glsl` · `view_types.glsl` · `packing.glsl` · `random.glsl` · `brdf.glsl` · `tonemap.glsl` · `color_spaces.glsl` · `shadow_sampling.glsl` · `light_common.glsl` · `gbuffer_layout.glsl` · `motion_vectors.glsl` · `bindless.glsl`

## 1. Utility (`utility/`)
`fullscreen_triangle` · `blit_color` · `blit_depth` · `clear_color` · `copy_rgba_to_nv12` · `generate_mips`

## 2. Visibility & GPU-driven (`visibility/`)
`hzb_build` · `gpu_frustum_cull` · `gpu_occlusion_cull` · `meshlet_cull` · `draw_compact` · `visibility_buffer_resolve`

## 3. Raster geometry (`raster/`)
- **Depth prepass**: `depth_prepass_static` · `depth_prepass_skinned` · `depth_prepass_alphatest`
- **G-buffer**: `gbuffer_static` · `gbuffer_skinned` · `gbuffer_instanced` · `gbuffer_alphatest` · `gbuffer_decal`
- **Forward**: `forward_opaque_static` · `forward_opaque_skinned`
- **Forward+**: `forward_plus_opaque_static` · `forward_plus_opaque_skinned`
- **Deferred lighting**: `deferred_lighting_analytic` · `deferred_lighting_clustered`
- **Transparency**: `forward_transparent` · `oit_accumulate` · `oit_resolve`
- **Mesh shaders (optional)**: `meshlet_task` · `meshlet_mesh` · `meshlet_frag`

## 4. Shadows (`shadow/`)
`csm_depth_static` · `csm_depth_skinned` · `csm_depth_alphatest` · `vsm_page_mark` · `vsm_page_alloc` · `vsm_render_static` · `vsm_render_skinned` · `vsm_hzb` · `pcss_filter` · `moment_shadow`

## 5. Lighting grids (`lighting/`)
`light_cluster_build` · `light_tile_build` · `light_assign` · `decal_cluster_build` · `probe_cluster_build`

## 6. Sky & atmosphere (`sky/`)
`transmittance_lut` · `multi_scattering_lut` · `sky_view_lut` · `aerial_perspective_lut` · `sky_render` · `sun_disk`

## 7. Volumetrics (`volumetric/`)
`froxel_inject` · `froxel_scatter` · `froxel_integrate` · `clouds_raymarch` · `clouds_reproject` · `fog_apply`

## 8. Global illumination (`gi/`)
- **DDGI**: `ddgi_trace` · `ddgi_probe_blend_irradiance` · `ddgi_probe_blend_depth` · `ddgi_probe_classify` · `ddgi_sample`
- **Lumen-style**: `lumen_surface_cache_update` · `lumen_screen_trace` · `lumen_world_trace` · `lumen_radiance_cache` · `lumen_reflection_gather` · `lumen_final_gather`
- **Radiance cascades**: `cascades_build` · `cascades_merge` · `cascades_sample`
- **SSGI / SSAO**: `ssao` · `ssao_blur` · `ssgi` · `ssgi_temporal`

## 9. Reflections (`reflections/`)
`ssr_hiz_trace` · `ssr_resolve` · `ssr_temporal` · `rt_reflections` (rgen/rchit/rmiss) · `restir_reflection_initial` · `restir_reflection_temporal` · `restir_reflection_spatial` · `reflection_probe_prefilter` · `reflection_probe_apply`

## 10. Ray tracing core (`rt/`)
`rt_common.glsl` (include) · `rt_shadows` (rgen/rmiss) · `rt_ao` (rgen/rchit/rmiss) · `rt_gi` (rgen/rchit/rmiss) · `rt_path_trace` (rgen/rchit/rmiss/rahit) · `rt_restir_di_initial` · `rt_restir_di_temporal` · `rt_restir_di_spatial`

## 11. IBL (`ibl/`)
`equirect_to_cube` · `irradiance_convolution` · `prefilter_environment` · `brdf_lut`

## 12. Temporal (`temporal/`)
`motion_vector_static` · `motion_vector_skinned` · `motion_vector_resolve` · `taa_resolve` · `history_invalidate` · `history_reproject`

## 13. Denoising (`denoise/`)
`svgf_temporal` · `svgf_atrous` · `svgf_moments` · `relax_temporal` · `relax_spatial` · `nrd_reblur_temporal` · `nrd_reblur_spatial` · `firefly_suppress`

## 14. Upscaling (`upscale/`)
`dlss_dispatch_setup` · `fsr3_easu` · `fsr3_rcas` · `xess_setup` · `tsr_dilate_velocity` · `tsr_reproject` · `tsr_resolve`

## 15. Post-process (`post/`)
`auto_exposure_histogram` · `auto_exposure_average` · `bloom_extract` · `bloom_downsample` · `bloom_upsample` · `bloom_composite` · `depth_of_field_coc` · `depth_of_field_gather` · `depth_of_field_scatter` · `motion_blur_tile_max` · `motion_blur_reconstruct` · `chromatic_aberration` · `lens_flare_generate` · `lens_flare_composite` · `vignette` · `film_grain` · `color_grading_lut` · `tonemap_aces` · `fxaa` · `smaa_edge` · `smaa_blend` · `smaa_neighborhood` · `display_encode_sdr` · `display_encode_hdr10`

## 16. Particles & hair (`particles/`, `hair/`)
`particle_simulate` · `particle_sort` · `particle_render_billboard` · `particle_render_mesh` · `hair_strand_expand` · `hair_marschner` · `hair_transparent_oit`

## 17. Water & terrain (`water/`, `terrain/`)
`water_fft_spectrum` · `water_fft_butterfly` · `water_normal_build` · `water_surface` · `water_caustics` · `terrain_patch_select` · `terrain_static` · `terrain_clipmap_update`

## 18. Decals (`decals/`)
`deferred_decal_apply` · `clustered_decal_apply`

## 19. UI & 2D (`ui/`, `2d/`)
`sprite` · `text_msdf` · `primitive_2d` · `ui_quad` · `imgui`

## 20. Point-based & neural (`neural/`, `splats/`)
`splat_sort` · `splat_project` · `splat_rasterize_tile` · `splat_composite` · `neural_field_sample` · `neural_radiance_cache_inference` · `neural_radiance_cache_train`

## 21. Editor/debug (`editor/`)
`debug_lines` · `debug_wireframe` · `debug_vector_field` · `debug_hzb_view` · `entity_id` · `outline_mask` · `outline_composite` · `overlay_2d` · `gizmo_lit`

## 22. VR / stereo (`vr/`)
`stereo_reproject` · `foveated_downsample` · `foveated_upsample` · `vrs_image_build`

## 23. Streaming / compute utilities (`streaming/`)
`texture_transcode` · `virtual_texture_feedback` · `virtual_texture_pageupload` · `rgba_to_yuv`

---

### Headline count

| Tier | Raster + shadows + post + IBL + editor | **~60 logical shaders** (baseline) |
|---|---|---|
| + GI, reflections, temporal, denoise, atmosphere, volumetrics | **~100** | (Lumen/DDGI-class feature set) |
| + RT, upscaling vendors, hair/water/terrain, particles | **~140** | (UE5-class breadth) |
| + Nanite meshlets, VSM, splats/neural, VR | **~180** | (complete industry-grade) |

Ship in order: **utility → visibility → raster → shadows → lighting → IBL → post → temporal → sky → reflections → RT → GI → denoise → upscale → neural**. Each tier's shaders go to disk only when the feature registers; capability-gated tracks never ship shaders on unsupported hardware.

**Last updated:** 2026-04-12  
**Status:** Planning — implementation starts at Phase 0.
