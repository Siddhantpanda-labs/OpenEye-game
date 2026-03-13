# OpenEye Render Refractor Plan (Limitless-Inspired)

Plan for improving OpenEye’s render pipeline and material system above the RHI, using the **Limitless engine** as reference. All changes stay **above RHI** and are **backend-agnostic** (OpenGL today; Vulkan or other backends later without redoing this work).

**Related docs:**

- `Previous_OpenEye_Architecture_And_Flow.md` — OpenEye layout and flow
- `Limitless_review.md` — Limitless pipeline, material, shader, lighting summary (source for §2)
- `EngineSource/eyelib/render/RENDER_MODULE.md` — render module layout

---

## 1. Goals

- **Stronger pipeline:** Clear split between *pipeline stage* (which pass, what output) and *material surface* (how the surface looks; optionally injectable code). Match Limitless pattern: passes use an instance renderer; shader selection by (stage × instance type × material type).
- **Stronger materials:** Materials as first-class surface definitions (shading model, properties, optional snippets), with a **material buffer** and **shader variants** per (pass × instance type × material type). Limitless: Material has Buffer (std140), shader_index, blending, vertex/fragment/shading snippets; MaterialCompiler compiles per (ShaderType, InstanceType) and stores in assets.
- **Above RHI only:** No changes to `eyelib/render/rhi/`. RHI remains backend-agnostic; all new logic lives in pipeline, passes, materials, and shader compilation/cache.
- **Preserve architecture:** Apps still use `api/` only; frame graph and `IRenderSceneView` stay; World → Snapshot → BuildScene → Render flow unchanged where possible.

### 1.1 Implementation practice (reference actual Limitless code)

When implementing any Limitless-inspired feature in OpenEye (including Phase 7+ and beyond):

1. **Use this plan and `Limitless_review.md`** for *what* to build and *where* it lives in Limitless (file/module names).
2. **Open and read the actual Limitless engine source** for that feature. Reference: **`EngineSource/thirdparty/limitless-engine/`** (e.g. `include/limitless/lighting/cascade_shadows.hpp`, `ms/material.hpp`, `renderer/instance_renderer.cpp`, `renderer/shader_type.hpp`).
3. **Adapt their logic and data flow** to OpenEye’s RHI, frame graph, and API—do not rely only on the conceptual summary. The real implementation is the authority for correctness and edge cases.
4. This reduces the risk of re-inventing or mis-implementing; use the real code for confidence that the design is applied correctly.

---

## 2. How Limitless Implements It (Summary)

*Source: `Limitless_review.md`; reference repo: `limitless-engine` (renderer, ms, core/shader, lighting).*

### 2.1 Pipeline

- **Renderer:** Holds a list of `RendererPass` and an `InstanceRenderer`. `render()` runs `update()` then `render()` on each pass in order. Builder adds passes (e.g. `addGBufferPass()`, `addDeferredLightingPass()`).
- **Deferred pipeline:** SceneUpdate → DirectionalShadow → DeferredFramebuffer → Depth → GBuffer → Decal → Skybox → SSAO → SSR → DeferredLighting → Translucent → Bloom → Composite → FXAA → Screen → Debug.
- **RendererPass:** Base: `update(Scene, Camera)`, `render(InstanceRenderer, Scene, Context, Assets, Camera, UniformSetter)`, `onFramebufferChange()`.
- **InstanceRenderer:** Draws scene instances per pass. For each mesh: get **Material** → get **ShaderProgram** from `assets.shaders.get(ShaderType, InstanceType, material->getShaderIndex())` → call **`shader.setMaterial(material)`** → set pass/instance uniforms → draw. So: **Pass** → **InstanceRenderer** → (ShaderType + InstanceType + material.shader_index) → **ShaderProgram** → **setMaterial** → draw.

### 2.2 Materials

- **Material:** Properties (e.g. color), **blending**, **shading model**, **two-sided/refraction**, **shader_index**, **model_shaders** (InstanceTypes), custom uniforms, **vertex/fragment/shading snippets**; has a **Buffer** (std140) packing uniforms for GPU.
- **MaterialCompiler:** Extends ShaderCompiler. For (Material, ShaderType, InstanceType): load pass shader template, apply material-defined replacements, store result in `assets.shaders` under (pass_shader, model_shader, material.shader_index).
- **MaterialBuilder:** Assigns shader_index, registers with Assets, sets properties and model shaders; `build(Assets)` creates Material.
- **MaterialShaderDefineReplacer:** Injects material-dependent defines into shader source before compile.
- **Blending:** Enum (e.g. Opaque vs transparent). **Shading** / **Property** enums for materials.

### 2.3 Shaders

- **Shader:** Single GLSL stage; loads source, `#include`, key replacement; compiles to GL shader id. Input to ShaderCompiler.
- **ShaderCompiler:** Path → ShaderProgram; applies RendererSettings defines. **MaterialCompiler** extends it for material variants.
- **ShaderProgram:** `use()`, `setUniform()`, **`setMaterial(Material)`** — binds material buffer and textures. What the pipeline uses at draw time.
- **ShaderType / InstanceType:** ShaderType = Forward, Depth, GBuffer, Skybox, DirectionalShadow, ColorPicker, Decal. InstanceType = Model, Skeletal, Instanced, … **ShaderKey** = (ShaderType, InstanceType, material_index). Pass paths in SHADER_PASS_PATH (e.g. pipeline/forward, pipeline/gbuffer, lighting/directional_shadows).
- **Shader storage:** Registry of compiled programs keyed by ShaderKey.

### 2.4 Lighting

- **Lighting:** One directional light, ambient, **LightContainer** for point/spot. Builds **scene_lighting SSBO** (direction, color, ambient, light_count); updates when changed; binds for pipeline.
- **Light:** Type (Directional, Point, Spot), color, position, direction, cone angles, radius; Builder API.
- **Cascade shadows:** Dedicated component for directional light cascade shadow maps; used by DirectionalShadowPass.

---

## 3. OpenEye vs Limitless Alignment

| Concept | Limitless | OpenEye (current) |
|--------|-----------|--------------------|
| Pass → shader selection | InstanceRenderer; get program by (ShaderType, InstanceType, material.shader_index) | Passes use ShaderCache; getOrCompile(device, ShaderKey{ stage, instanceType, materialTypeId }, material) |
| Shader key | (ShaderType, InstanceType, material_index) | (PipelineStage, InstanceType, materialTypeId) — same idea |
| Material buffer | Material has Buffer (std140); setMaterial uploads | MaterialBlockStd140 UBO at binding 2; SetMaterial in draw path |
| MaterialCompiler | Load pass template, replace defines/snippets, compile, store in assets.shaders | Load template, getMaterialDefines, replace @MATERIAL_*@, BuildFromSource, store in ShaderCache |
| Scene lighting | scene_lighting SSBO; one place, bind for passes | SceneLightingBlock SSBO (binding 5); FillSceneLightingBuffer; deferred, forward, forward+ use it |
| Shading model / snippets | Lit/Unlit/Custom; vertex/fragment/shading snippets | ShadingModel enum; fragment/shading snippets; property defines (HAS_ALBEDO_TEXTURE, etc.) |
| Shadow pass | DirectionalShadowPass; cascade shadows | ShadowPass; single shadow map; templates + ShaderCache |
| Blending / two-sided / refraction | Material: blending, two-sided, refraction | Not yet (data-only + shading model + snippets) |
| Instance types | Model, Skeletal, Instanced, Decal, … | Static, Skinned (Instanced/Decal not in key yet) |
| Cascade shadows | cascade_shadows.hpp/cpp for directional | Not yet; single shadow map |
| Hot-reload | — | reloadAll() clears ShaderCache; next getOrCompile recompiles from templates |

---

## 4. Original Gaps (Pre-Refractor)

| Area | Then | Now |
|------|------|-----|
| Pipeline vs material | One shader per pass; material = data only | Stage vs surface split; key-driven variants; MaterialCompiler |
| Shader selection | Pass binds one shader; per-draw material data | ShaderKey → ShaderCache → getOrCompile; batch by key |
| Material model | No shading model, no snippets | ShadingModel (Lit/Unlit/Custom); snippets; property defines |
| Instance types | Branch (skinned vs not) | InstanceType in ShaderKey; Static/Skinned |
| Lighting | Per-pass light uniforms | Single scene lighting SSBO (binding 5) for deferred, forward, forward+ |

---

## 5. Target Architecture (Limitless-Inspired)

### 5.1 Conceptual Split

- **Pipeline shader (stage):** Defines the *rendering stage* — Forward, GBuffer, Depth, DirectionalShadow, Lighting (full-screen), Skybox. Implemented as pass shaders in `eyelib/render/passes/` and frame graph.
- **Material shader (surface):** Defines *surface behavior* — albedo, normal, metallic, roughness, emission, opacity, custom shading. Either data-only (material feeds one shared pass shader) or code-injected (snippets / shading model compiled into pass template via placeholders → one program per (pass, instance type, material type)).

### 5.2 Shader Key and Variants

- **ShaderKey** = (PipelineStage, InstanceType, MaterialTypeOrIndex). PipelineStage = Forward, GBuffer, Depth, etc.; InstanceType = Static, Skinned (later: Instanced, Decal); MaterialTypeOrIndex = material’s shader_index.
- **ShaderCache:** getOrCompile(device, key [, material]) → compiled RHI pipeline. Populated by **MaterialCompiler** (template + defines/snippets → compile → cache).
- **Draw time:** Pass gets program(ShaderKey), binds it, **SetMaterial(material)** (buffer + textures), sets per-draw uniforms, draws. Drawables batched by ShaderKey.

### 5.3 Material System

- **Material:** Properties, textures, material buffer (std140). Optional: ShadingModel, shader_index, fragment_snippet, shading_snippet. (Future: blending, two-sided, refraction — Limitless-aligned.)
- **MaterialCompiler:** (Material, PipelineStage, InstanceType) → load template → defines + placeholders → compile → cache under ShaderKey.
- **Backward compatibility:** Default materials use default shader_index and no snippets; one “default” variant per (stage, instance type).

### 5.4 Pipeline and Passes

- **Frame graph:** Unchanged in spirit (GpuCull → Shadow → GBuffer → Lighting → …).
- **Passes that draw geometry:** Use ShaderCache + ShaderKey; SetMaterial(material); batch by key. Non-draw passes (lighting, post) use their shaders; lighting consumes scene lighting SSBO.

### 5.5 Lighting

- **Scene lighting buffer:** Single SSBO (binding 5): header (directional, ambient, exposure, shadow) + point/spot arrays. One fill per frame; deferred, forward, forward+ bind it. **Cascade shadows:** To be added later (Limitless: cascade_shadows + DirectionalShadowPass).

### 5.6 What Stays Unchanged

- **RHI:** No changes. **API:** Stable; extend only where needed (e.g. api::Material options). **App flow:** World → Snapshot → BuildScene → Render. **IRenderSceneView:** Zero-copy; pipeline consumes it.

---

## 6. Implementation Phases (Done: 1–5, 6.1–6.4)

### Phase 1: Shader key and cache

- PipelineStage, InstanceType, ShaderKey in `ShaderKey.h`. ShaderCache: getOrCompile(device, key). GBufferPass: bucket by key, get program from cache, SetMaterial, draw.

### Phase 2: Material buffer and SetMaterial

- Material buffer (std140), GetMaterialBufferRHI, EnsureMaterialBuffer. Draw path: BindUniformBuffer(materialBuffer, …); shaders use MaterialBlock UBO.

### Phase 3: Material compiler and placeholders

- Pass templates with `@MATERIAL_DEFINES@`, `@MATERIAL_FRAGMENT_SNIPPET@`, `@MATERIAL_SHADING_SNIPPET@`. MaterialCompiler loads template, replaces, compiles, cache. Material: shader_index, snippets.

### Phase 4: Shading model and property defines

- ShadingModel (Lit, Unlit, Custom); MaterialCompiler emits SHADING_* and HAS_* defines; templates branch. Property defines (HAS_ALBEDO_TEXTURE, HAS_NORMAL_MAP, …).

### Phase 5: Lighting buffer and cleanup

- SceneLighting SSBO (binding 5), FillSceneLightingBuffer. LightingPass and deferred use it. Docs updated.

### Phase 6.1–6.4 (enhancements)

- **6.1:** Forward/Forward+ use scene lighting SSBO; forward shaders renamed to forward / forward_skinned.
- **6.2:** Shadow pass templates + placeholders; ShadowPass uses ShaderCache only.
- **6.3:** Hot-reload: ShaderCache::reloadAll() clears cache; next getOrCompile recompiles from templates.
- **6.4:** api::Material exposes ShadingModel, GetShaderIndex/SetShaderIndex, fragment/shading snippet get/set.

---

## 7. File and Module Ownership

- **ShaderKey, PipelineStage, InstanceType:** `eyelib/render/ShaderKey.h`
- **ShaderCache:** `eyelib/render/ShaderCache.h/cpp`; pipeline-owned; populated by MaterialCompiler
- **MaterialCompiler:** `eyelib/render/MaterialCompiler.h/cpp`
- **Material buffer / SetMaterial:** `eyelib/render/resources/Material.h/cpp`; draw path in GBufferPass, ShadowPass
- **Pass shader templates:** `EngineSource/assets/shaders/opengl/` (e.g. passes/deferred, passes/shadow, passes/forward)
- **Scene lighting:** `eyelib/render/SceneLighting.h/cpp`; filled from scene; bound at 5

---

## 8. Success Criteria

- Pipeline and material responsibilities clearly separated (stage vs surface).
- Shader selection key-driven (pass × instance type × material type); variants compiled and cached above RHI.
- Materials can be data-only or opt in to shading model and snippets.
- No RHI/backend API changes; design works with OpenGL and future backends.
- Existing apps and content keep working; new features additive and optional.

---

## 9. References

- **Limitless:** `EngineSource/thirdparty/limitless-engine/` (source); `documentation/refractor/Limitless_review.md` (summary: pipeline, material, ShaderKey, lighting).
- **OpenEye:** `documentation/refractor/Previous_OpenEye_Architecture_And_Flow.md`, `EngineSource/eyelib/render/RENDER_MODULE.md`.

---

## 10. Implementation Progress

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| **Phase 1** | PipelineStage, InstanceType, ShaderKey | Done | `eyelib/render/ShaderKey.h` |
| **Phase 1** | ShaderCache getOrCompile | Done | GBuffer + Depth keys; resolve to pass shader names |
| **Phase 1** | GBufferPass key + cache, batch by key | Done | SetShaderCache; bucket by key; getOrCompile; drawOne with SetMaterial |
| **Phase 2** | Material buffer (std140) | Done | MaterialBlockStd140 UBO binding 2; EnsureMaterialBuffer |
| **Phase 2** | SetMaterial in draw path | Done | GBufferPass drawOne: BindUniformBuffer(materialBuffer, …) |
| **Phase 3** | Pass templates + placeholders | Done | gbuffer_skinned.frag, shadow.frag, shadow_skinned.frag: @MATERIAL_*@ |
| **Phase 3** | MaterialCompiler | Done | Load template, getMaterialDefines, replace placeholders, BuildFromSource; GBuffer + Depth |
| **Phase 3** | Material shader_index + snippets | Done | ShaderCache getOrCompile(device, key, material); GBufferPass keys by material->GetShaderIndex() |
| **Phase 4** | Shading model + defines | Done | ShadingModel enum; SHADING_LIT/UNLIT/CUSTOM; gbuffer/lighting branch |
| **Phase 4** | Property defines | Done | HAS_ALBEDO_TEXTURE, HAS_NORMAL_MAP, etc.; default key uses getDefaultMaterial() |
| **Phase 5** | Scene lighting buffer | Done | SceneLightingBlock SSBO binding 5; FillSceneLightingBuffer; LightingPass, forward, forward+ |
| **Phase 5** | Docs + cleanup | Done | Refractor plan; RENDER_MODULE.md |
| **Phase 6.1** | Forward+ scene lighting buffer | Done | RenderForwardPlus fill/bind at 5; forwardplus.*.frag read from SSBO |
| **Phase 6.1** | Forward (lit→forward, skinned→forward_skinned) | Done | forward.*, forward_skinned.* use SSBO (5), shadow (6); call sites updated |
| **Phase 6.2** | Shadow templates + ShaderCache | Done | shadow.frag/skinned: placeholders; ShadowPass cache-only; bucket by ShaderKey(Depth, …) |
| **Phase 6.3** | Hot-reload MaterialCompiler variants | Done | ShaderCache::reloadAll() clears cache; next getOrCompile recompiles from templates |
| **Phase 6.4** | API material options | Done | api::Material: ShadingModel, GetShaderIndex/SetShaderIndex, GetFragmentSnippet/SetFragmentSnippet, GetShadingSnippet/SetShadingSnippet |
| **Phase 7.1** | Cascade shadows | Done | CascadeShadowMap (Limitless-inspired), RHI depth-array RT, ShadowPass per-cascade, LightingPass + lighting.frag CSM; forward use array layer 0 |
| **Phase 7.2** | Material blending / two-sided / refraction | Done | Blending enum; two_sided, refraction on Material; MaterialCompiler HAS_* defines; SetBlendFunc; blend/cull in GBuffer, Shadow, Transparency passes; api Material API. |
| **Phase 7.3** | Instanced instance type | Done | InstanceType::Instanced; gbuffer_instanced, shadow_instanced templates (SSBO binding 4); GBufferPass::RenderInstanced, ShadowPass::RenderInstanced. |
| **Phase 7.4** | Post-processing (SSAO, SSR, Bloom) | Done | PostProcessor already had SSAO, SSR, Bloom; aligned with Limitless: SSR view-space ray march + depth buffer + correct projection; SSAO bilateral blur (two-pass, depth-aware); SSAO occlusion condition fixed (depthDiff behind surface). |
| **Asset** | Canonical .mat, remove .mat JSON | Done | Canonical material extension is .mat (binary); .material removed from importer/materialmap; MaterialLoader/MaterialReader/docs say canonical .mat binary; .mat JSON removed (no reader ever used it). |
| **Phase 7.5** | MaterialBuilder | Done | eyelib/render/resources/MaterialBuilder.h/.cpp: fluent API (Albedo, Metallic, Roughness, textures, Blending, TwoSided, Refraction, ShaderIndex, snippets, etc.); build() → shared_ptr&lt;Material&gt;. Loader path unchanged (LoadedMaterial → MaterialDescriptor → CreateMaterialFromDescriptor). |

---

## 11. Phase 7+ (Limitless-Aligned Next Enhancements)

Follow the Limitless design for the following; implement when needed. Order is suggestive.

| # | Enhancement | Limitless reference | Goal | Status |
|---|-------------|---------------------|------|--------|
| **7.1** | Cascade shadows | `lighting/cascade_shadows.hpp/cpp`; DirectionalShadowPass | Multiple cascades for directional light; shadow pass and lighting sample cascades. | Done | RHI: CreateDepthOnlyRenderTargetArray + SetDepthLayer on IRenderTarget (OpenGL). CascadeShadowMap: 4 cascades, Texture2DArray depth, SSBO light-space matrices, Limitless-style split weight and crop. ShadowPass::Render(CascadeShadowMap&) loops cascades, BindForShadowPass(cmd, i), draw. SceneLightHeader: cascadeCount, farBounds; FillSceneLightingBuffer(cascadeMap); LightingPass binds array at 6, light-space SSBO at 7, u_ViewProj for cascade selection. lighting.frag: sampler2DArray, cascade selection by NDC z, sample lightSpace[c]. Forward shaders: sampler2DArray, layer 0. Pipeline uses m_cascadeShadowMap; GetShadowMap() returns nullptr. |
| **7.2** | Material blending / two-sided / refraction | `ms/material.hpp` (blending, two-sided, refraction); `ms/blending.hpp` | Add blending mode (Opaque/Translucent), two-sided, refraction to Material; defines in MaterialCompiler; template branches. | Done | Blending enum (Opaque, Translucent, Additive, Modulate) in eyelib + api Material. TwoSided and Refraction bools. MaterialCompiler: HAS_REFRACTION, HAS_TWO_SIDED defines. RHI: SetBlendFunc(src,dst). GBufferPass, ShadowPass, TransparencyPass: applyMaterialBlendAndCull (blend + depth write + cull from material). api::Material: GetBlending/SetBlending, GetTwoSided/SetTwoSided, GetRefraction/SetRefraction. |
| **7.3** | Instanced (and optional Decal) instance type | `shader_type.hpp`: InstanceType = Model, Skeletal, **Instanced**, **Decal** | Add InstanceType::Instanced (and optionally Decal) to ShaderKey; pass templates and cache support; draw path branches. | Done | InstanceType::Instanced in ShaderKey. MaterialCompiler: gbuffer_instanced, shadow_instanced templates. Shaders: SSBO binding 4 (InstanceModels), gl_InstanceID. StorageBinding::InstanceMatrix = 4. GBufferPass::RenderInstanced, ShadowPass::RenderInstanced (mesh + material + instanceMatrixBuffer + count). Decal deferred. |
| **7.4** | Optional pipeline passes | `deferred.cpp`: SSAO, SSR, Bloom, Composite, FXAA, … | Add SSAO, SSR, Bloom, or other post passes as in Limitless deferred pipeline; reuse frame graph and scene lighting where applicable. | Done | PostProcessor: SSAO (fixed occlusion condition + bilateral blur), SSR (view-space ray + g_Depth + near/far), Bloom; pipeline passes already present. |
| **7.5** | MaterialBuilder / loader / serialization | `ms/material_builder.hpp`, `loaders/material_loader.hpp`, `serialization/material_serializer.hpp` | Optional: builder API for materials (shader_index, properties, model shaders); asset loader and serialization for materials. | Done | MaterialBuilder in `eyelib/render/resources/MaterialBuilder.h/.cpp`: fluent API (Albedo, Metallic, Roughness, Emission, AO, Alpha, SetTexture, SetShader, SetFloat/Vec3/Vec4/Mat4Array, ShadingModel, ShaderIndex, FragmentSnippet, ShadingSnippet, Blending, TwoSided, Refraction, Name); build() returns shared_ptr&lt;Material&gt;. Loader path unchanged: LoadedMaterial → MaterialDescriptor → CreateMaterialFromDescriptor. |

---

### Current status (what's done, what's next)

- **Done:** Phases 1–6.4, **7.1**–**7.5**, **Asset** (canonical .mat, .mat JSON removed). **7.5:** MaterialBuilder fluent API; build() → Material; loader path via existing descriptor/resolver.
- **Limitless reference:** `EngineSource/thirdparty/limitless-engine/` (used for 7.1–7.5).
- **Next:** Further enhancements as needed (e.g. Decal instance type, additional serialization).

---

*Document updated to reflect Limitless implementation study and OpenEye alignment; Phase 7+ defines the next enhancement roadmap.*
