# Deferred Raster Architecture (`features/raster`)

This document is the practical architecture guide for the deferred path implemented in the new rendering module.

Scope: `EngineSource/eyelib/rendering/**` (new module only).  
Primary feature owner: `features/raster/RasterFeature.*`.

---

## 1) Design intent

Deferred rendering is implemented as a feature-owned, framegraph-driven pipeline with strict separation of concerns:

- `core/RenderWorld` orchestrates frame phases only.
- `features/raster` owns raster pass scheduling and mode selection.
- `framegraph` owns pass order, barrier planning, and execution.
- `core/RenderResources` owns long-lived GPU resources and upload lifecycle.
- `FrameGraphBlackboard` is the only resource exchange channel between subsystems/passes.

No deferred pass writes GPU work outside framegraph execution.

---

## 2) Runtime flow (single frame)

Authoritative order in `RenderWorld::RunFrame`:

1. `UploadPhase`:
   - visibility builds typed draw list
   - resources are ensured/published
   - feature `OnUpload` records uploads
2. `FrameScheduler::Schedule`:
   - feature `OnFramePrepare`
   - feature `OnSchedulePasses` (adds passes to graph)
3. `FrameGraph::Compile`:
   - pass barrier collection
   - dedupe and merge against tracker
4. `FrameGraph::Execute`:
   - barrier replay
   - pass execute in deterministic order

In deferred mode, raster schedules:

- `GBufferPass`
- `DeferredLightingPass`
- `BlitMainHdrToDefaultPass`

---

## 3) Folder and ownership map (deferred-specific)

- `features/raster/RasterFeature.*`
  - chooses mode (forward/deferred)
  - warms shader pipelines
  - owns persistent pass objects
  - publishes IBL textures to blackboard
- `features/raster/GBufferPass.*`
  - writes GBuffer MRT + depth
  - consumes draw list + mesh resolver + material/per-object buffers
- `features/raster/DeferredLightingPass.*`
  - reads GBuffer + depth + dir light + IBL
  - writes HDR color
- `framegraph/passes/BlitMainHdrToDefaultPass.*`
  - common final blit/present helper
- `core/RenderResources.*`
  - allocates GBuffer/HDR/depth RTs
  - owns PerFrame/PerView/PerObject/MaterialTable uploads
- `materials/MaterialSchema.h`, `materials/MaterialTable.*`
  - CPU record + GPU std430 contract for material rows

Raster-specific deferred passes live in `features/raster` by design.  
Framegraph pass folder is reserved for common/infrastructure-neutral passes.

---

## 4) Deferred pipeline contracts (frozen)

## 4.1 `RasterGBuffer` contract

Pipeline layout sets:

- `set 0`: `Frame`
  - `binding 0`: PerFrame UBO
  - `binding 1`: PerView UBO
- `set 1`: `RasterGBufferData`
  - `binding 0`: PerObject SSBO
  - `binding 1`: MaterialTable SSBO
- `set 2`: bindless textures
  - `binding 0`: `sampler2D uBindlessTextures[]`

Push constants:

- `uint objectIndex`
- `uint materialIdOverride` (`0xFFFFFFFFu` = disabled; use per-object material id)

Attachment outputs:

- RT0 `RGBA8`: albedo + packed flags
- RT1 `RGBA16F`: world normal + emissive strength
- RT2 `RGBA8`: metallic/roughness/ao/mask
- Depth `Depth24Stencil8`

Attachment ops:

- color: clear + store
- depth: clear + store

## 4.2 `RasterDeferredLighting` contract

Pipeline layout sets:

- `set 0`: `Frame`
- `set 1`: `RasterDeferredLightingData`
  - `binding 0`: `tGBuffer0`
  - `binding 1`: `tGBuffer1`
  - `binding 2`: `tGBuffer2`
  - `binding 3`: `tDepth` (GBuffer depth)
  - `binding 4`: directional light UBO
  - `binding 5`: irradiance cubemap
  - `binding 6`: prefilter cubemap
  - `binding 7`: BRDF LUT

Geometry:

- fullscreen triangle (no vertex buffer layout)
- no pass push constants

Attachment outputs:

- HDR color `RGBA16F`
- no depth attachment (depth is sampled texture only)

Attachment ops:

- color: clear + store

---

## 5) Data contracts between CPU and shaders

Material table GPU row (`std430`) is canonical and shared by CPU packing + shader reads.

- CPU pack source: `materials/MaterialSchema.h`
- Shader read site: `assets/shaders/rendering/vulkan/raster/deferred/gbuffer_static.vert`

Texture handles are authored by CPU material packing and consumed via bindless in
`gbuffer_static.frag`.

Invalid handle rules:

- `0xFFFFFFFFu` and `0u` are treated as invalid in shader
- shader uses safe fallback values (no undefined reads)

---

## 6) Blackboard ownership (deferred path)

Core writer (`writer_names::kRenderResources`) publishes:

- `MainHdrRenderTarget`
- `GBufferRenderTarget`
- `MainDepth`
- `PerFrameUniformBuffer`
- `PerViewUniformBuffer`
- `PerObjectStorageBuffer`
- `MaterialTableStorageBuffer`
- `DirectionalLightUniformBuffer`

Raster IBL writer (`writer_names::kLightingIbl`) publishes:

- `IblIrradianceCubemap`
- `IblPrefilterCubemap`
- `IblBrdfLut`

Single-writer invariants are validated by the writer registry (debug assertions).

---

## 7) Barriers and correctness model

Each pass declares texture intent via `CollectTextureBarriers`.

`FrameGraph::Compile` performs:

- per-pass dedupe (`DedupeConsecutiveSameTargetState`)
- merge against tracker (`MergeBarrierPlansAgainstTracker`)
- debug validation warnings for risky undefined-first-read and inter-pass layout gaps

No pass manually owns cross-pass barrier sequencing logic.

---

## 8) Shader source policy and `.spv` workflow

Raster feature uses `ShaderSourcePolicy::PreferSpirv` for deferred shaders.

Implication:

- runtime prefers `.spv` when present
- source edits require compile pipeline to refresh `.spv`

Tooling:

- `EngineSource/tools/compile_vulkan_shaders.py`
- persistent state file:
  - `EngineSource/assets/shaders/vulkan_shader_compile_state.json`
- supports incremental compile and `--reset-state`

---

## 9) What this architecture prevents

- no cross-feature direct resource grabbing
- no pass-owned ad-hoc uploads in execute
- no legacy module include coupling
- no framegraph-bypass GPU writes
- no ambiguous ownership of deferred pass files

---

## 10) Extension guidance

When adding new raster deferred capabilities:

- keep pass logic in `features/raster`
- keep cross-pass resource contracts in blackboard slots and this document
- keep data layout changes synchronized across:
  - `MaterialSchema.h`
  - shader structs in `gbuffer_static.vert`
  - pack/read fallback behavior in `gbuffer_static.frag`
- update `documentation/Render_Module/Render_steps_todo.md` and `render_check.md`

This preserves clean layering and makes future tracks (Nanite/splats/NeRF/GI) composable rather than intertwined.

---

## 11) Deferred additions not yet implemented (next candidates)

The current deferred path is functional and architecturally clean. The following are meaningful next steps that are **not yet fully landed** in this module:

- **Deferred light scalability**
  - clustered/tiled deferred light list integration
  - many-light stress path with bounded CPU overhead and predictable descriptor usage

- **Decal integration**
  - deferred decal pass writing into GBuffer-compatible channels
  - explicit ordering contract with gbuffer + deferred lighting passes

- **Transparency strategy around deferred**
  - hybrid composition path (deferred opaque + forward transparent/alpha-tested variants)
  - OIT path definition and integration boundaries

- **Temporal stability improvements**
  - motion-vector-aware temporal accumulation for deferred output
  - compatibility rules with post stack and camera-cut invalidation

- **Post-lighting quality passes (deferred-aligned)**
  - SSAO/SSGI/SSR passes with explicit framegraph contracts and ownership
  - quality/perf tiers driven by capability + settings

- **Performance instrumentation and budgets**
  - pass-level budget targets for GBuffer and DeferredLighting
  - descriptor churn counters and upload-cost budget reporting in profiling output

- **Contract-level automation**
  - automated checks for set/binding contract drift (CPU layout vs shader layout)
  - CI smoke run that recompiles required `.spv` and validates fallback behavior

- **Feature extensions**
  - shadow strategy growth under `lighting/shadows` feeding deferred lighting
  - GI/reflection strategy growth under `lighting/gi` and `lighting/reflections` with blackboard-only integration

These should be added as incremental milestones, keeping the same rules: feature-owned passes, framegraph-only GPU writes, upload-phase discipline, and blackboard single-writer contracts.

