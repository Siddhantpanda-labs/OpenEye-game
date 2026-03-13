
Here’s a concise report of the limitless-engine layout and how pipeline, materials, shaders, and lighting fit together.

---

## Reference: limitless-engine

**Location:** **`EngineSource/thirdparty/limitless-engine/`** (include/ and src/ under that root).

**Layout:** Headers under **`include/limitless/`**, implementation under **`src/limitless/`**. Pipeline code is referenced as **`limitless/pipeline/`** (e.g. `deferred.hpp`, `common/dof_pass.hpp`) but that subtree was not found by glob; the design is clear from `deferred.cpp` and renderer usage.

### Directories under `src/limitless/` (and matching `include/limitless/`)

| Directory | Role |
|-----------|------|
| **renderer/** | Render pipeline: passes, instance rendering, deferred/forward. |
| **ms/** | Material system: Material, MaterialBuilder, MaterialCompiler, blending, shading. |
| **core/** | Shaders (Shader, ShaderCompiler, ShaderProgram), buffers, textures, uniforms, context. |
| **lighting/** | Lighting, Light, LightContainer, cascade shadows. |
| **instances/** | Instance types (mesh, model, skeletal, decal, effect, terrain, etc.). |
| **loaders/** | Asset/material/effect/texture/model loaders. |
| **models/** | Mesh/model types (cube, plane, skeletal, etc.). |
| **fx/** | Effects: emitters, effect compiler/builder/renderer. |
| **postprocessing/** | Bloom, blur, SSAO, SSR. |
| **serialization/** | Material/effect/emitter serialization. |
| **util/** | Thread pool, frustum, sorter, etc. |
| **text/** | Text/font atlas. |
| **skybox/** | Skybox. |
| **camera**, **scene**, **assets**, **shader_storage**, **log** | Top-level scene/assets/camera. |

Also: **samples/** (terrain, materials, lighting, effects, gltf_viewer), **tests/** (limitless unit tests).

---

## 1) Pipeline / render pipeline

- **`include/limitless/renderer/renderer.hpp`**  
  **`src/limitless/renderer/renderer.cpp`**  
  Main renderer: holds a list of `RendererPass` and an `InstanceRenderer`. `render()` runs `update()` on each pass then `render()` in order; `Builder` adds/removes passes (e.g. `addGBufferPass()`, `addDeferredLightingPass()`, `deferred()`).

- **`src/limitless/renderer/deferred.cpp`**  
  Deferred pipeline implementation: extends `Pipeline` (from `limitless/pipeline/deferred/deferred.hpp`), builds the pass sequence (SceneUpdate → DirectionalShadow → DeferredFramebuffer → Depth → GBuffer → Decal → Skybox → SSAO → SSR → DeferredLighting → Translucent → Bloom → Composite → FXAA → Screen → Debug).

- **`include/limitless/renderer/renderer_pass.hpp`**  
  Base for a single pipeline stage: `update(Scene, Camera)`, `render(InstanceRenderer, Scene, Context, Assets, Camera, UniformSetter)`, `onFramebufferChange()`.

- **`include/limitless/renderer/instance_renderer.hpp`**  
  **`src/limitless/renderer/instance_renderer.cpp`**  
  Draws scene instances per pass: selects shader by `ShaderType` + `InstanceType` + material `shader_index`, sets state (culling, blending), calls `shader.setMaterial(material)`, then draws. Used by passes to render geometry.

- **Key passes (all in `renderer/`):**  
  `gbuffer_pass`, `deferred_framebuffer_pass`, `deferred_lighting_pass`, `shadow_pass`, `depth_pass`, `skybox_pass`, `ssao_pass`, `ssr_pass`, `bloom_pass`, `composite_pass`, `translucent_pass`, `decal_pass`, `screen_pass`, `outline_pass`, `render_debug_pass`, `color_picker`, `fxaa_pass`, `sceneupdate_pass`, `scene_data`, `renderer_settings`, `render_settings_shader_definer`.

Pipeline and materials connect where **each pass** uses **InstanceRenderer** with a **ShaderType**; InstanceRenderer looks up the **ShaderProgram** by (ShaderType, InstanceType, material->shader_index) and calls **setMaterial** to push material data into that shader.

---

## 2) Material system

- **`include/limitless/ms/material.hpp`**  
  **`src/limitless/ms/material.cpp`**  
  Material: properties (e.g. color), blending, shading model, two-sided/refraction, `shader_index`, `model_shaders` (InstanceTypes), custom uniforms and vertex/fragment/shading snippets; has a `Buffer` that packs uniforms for the GPU (std140). “Visual look of a surface,” compiled via MaterialCompiler for use in the pipeline.

- **`include/limitless/ms/material_builder.hpp`**  
  **`src/limitless/ms/material_builder.cpp`**  
  Builder for materials: assigns `shader_index` (unique per “material type” or custom), registers with Assets, sets properties (e.g. `color()`), model shaders; `build(Assets)` creates the Material and adds it to `assets.materials`.

- **`include/limitless/ms/material_compiler.hpp`**  
  **`src/limitless/ms/material_compiler.cpp`**  
  Extends ShaderCompiler: compiles a Material for a given (ShaderType, InstanceType) by loading the pass shader, applying material-defined replacements, and storing the result in `assets.shaders` under (pass_shader, model_shader, material.shader_index). Pipeline uses these compiled shaders when drawing.

- **`include/limitless/ms/unique_material.hpp`**, **`src/limitless/ms/unique_material.cpp`**  
  UniqueMaterial: identifies material “type” (e.g. set of properties + shading) for shader indexing.

- **`include/limitless/ms/material_shader_define_replacer.hpp`**, **`src/limitless/ms/material_shader_define_replacer.cpp`**  
  Injects material-dependent defines into shader source before compile.

- **`include/limitless/ms/blending.hpp`**, **`src/limitless/ms/blending.cpp`**  
  Blending mode (e.g. Opaque vs transparent).

- **`include/limitless/ms/shading.hpp`**, **`include/limitless/ms/property.hpp`**  
  Shading model and property enums used by materials.

- **`include/limitless/loaders/material_loader.hpp`**, **`src/limitless/loaders/material_loader.cpp`**  
  Loads materials from assets.

- **`include/limitless/serialization/material_serializer.hpp`**, **`src/limitless/serialization/material_serializer.cpp`**  
  Serialization for materials.

Materials drive **which shader** is used (via `shader_index` and MaterialCompiler) and **what data** is sent (via `ShaderProgram::setMaterial` and Material’s buffer).

---

## 3) Shader system

- **`include/limitless/core/shader/shader.hpp`**  
  **`src/limitless/core/shader/shader.cpp`**  
  Single GLSL shader (vertex, fragment, etc.): loads source, supports `#include`, key replacement, and ShaderAction; compiles to a GL shader id. Used as input to ShaderCompiler, not the final program.

- **`include/limitless/core/shader/shader_compiler.hpp`**  
  **`src/limitless/core/shader/shader_compiler.cpp`**  
  Builds ShaderProgram from Shaders at a path: compiles and links, applies RendererSettings-driven defines. Base class for MaterialCompiler.

- **`include/limitless/core/shader/shader_program.hpp`**  
  **`src/limitless/core/shader/shader_program.cpp`**  
  Final linked program: `use()`, `setUniform()`, **`setMaterial(Material)`**; binds indexed buffers and textures. This is what the pipeline and InstanceRenderer use when drawing.

- **`include/limitless/core/shader/shader_define_replacer.hpp`**, **`src/limitless/core/shader/shader_define_replacer.cpp`**  
  Replaces keys in shader source (e.g. for quality/settings).

- **`include/limitless/renderer/shader_type.hpp`**  
  Defines **ShaderType** (Forward, Depth, GBuffer, Skybox, DirectionalShadow, ColorPicker, Decal) and **InstanceType** (Model, Skeletal, Instanced, …); **ShaderKey** = (ShaderType, InstanceType, material_index). Pass paths live in `SHADER_PASS_PATH` (e.g. pipeline/forward, pipeline/gbuffer, lighting/directional_shadows).

- **`include/limitless/core/shader/shader_program_introspection.hpp`**, **`shader_program_texture_setter.cpp`**  
  Introspection and texture binding for ShaderProgram.

- **`include/limitless/shader_storage.hpp`** (and .cpp)  
  Shader storage / registry.

So: **Shader** = one GLSL stage; **ShaderCompiler** = path → ShaderProgram; **MaterialCompiler** = Material + pass + instance type → ShaderProgram (stored in assets); **ShaderProgram** = the object used at render time and given material data via **setMaterial**.

---

## 4) Lighting

- **`include/limitless/lighting/lighting.hpp`**  
  **`src/limitless/lighting/lighting.cpp`**  
  Scene lighting: one directional light, ambient color, and a LightContainer for punctual lights; builds a “scene_lighting” SSBO (SceneLighting: direction, color, ambient, light_count), updates it when changed, and binds it for the pipeline.

- **`include/limitless/lighting/light.hpp`**  
  **`src/limitless/lighting/light.cpp`**  
  Single light: Type (Directional, Point, Spot), color, position, direction, cone angles, radius; Builder API, change tracking; one directional per scene, rest in LightContainer.

- **`include/limitless/lighting/light_container.hpp`**  
  **`src/limitless/lighting/light_container.cpp`**  
  Container for point/spot lights (add, update, visibility), used by Lighting to fill the scene lighting buffer.

- **`include/limitless/lighting/cascade_shadows.hpp`**  
  **`src/limitless/lighting/cascade_shadows.cpp`**  
  Cascade shadow maps for the directional light (used by DirectionalShadowPass).

Lighting is scene state; the **pipeline** consumes it via the shared “scene_lighting” buffer and the directional shadow pass.

---

## How pipeline and material relate (from .hpp/.cpp)

- **Renderer** runs a list of **RendererPass**es. Each pass that draws geometry uses **InstanceRenderer** with a **ShaderType** (and blending, etc.).
- **InstanceRenderer::render** (e.g. for ModelInstance): for each mesh it gets the **Material**, then gets the **ShaderProgram** from `assets.shaders.get(ShaderType, InstanceType, material->getShaderIndex())` — i.e. the program produced by **MaterialCompiler** for that material and that pass/instance type.
- It then calls **`shader.setMaterial(*mesh.getMaterial())`** so the **ShaderProgram** uploads that material’s uniforms/buffer; then it sets pass-specific and instance-specific uniforms and draws.
- So: **Pipeline** (Renderer + passes) → **Pass** → **InstanceRenderer** → (ShaderType + InstanceType + **Material.shader_index**) → **ShaderProgram** → **setMaterial(Material)** → draw. Materials are compiled per (pass, instance type) into ShaderPrograms; the pipeline only references materials through instances and uses the precompiled programs keyed by material.

---

## Key file paths (summary)

| Component | Path |
|-----------|------|
| Render pipeline | `include/limitless/renderer/renderer.hpp`, `renderer.cpp`; `src/limitless/renderer/deferred.cpp`; `renderer_pass.hpp`; `instance_renderer.hpp/cpp` |
| Passes | `include/` & `src/limitless/renderer/{gbuffer_pass,deferred_lighting_pass,shadow_pass,...}.hpp/cpp` |
| Material | `include/limitless/ms/material.hpp`, `material_builder.hpp`, `material_compiler.hpp`; same under `src/limitless/ms/` |
| Shader | `include/limitless/core/shader/shader.hpp`, `shader_compiler.hpp`, `shader_program.hpp`; same under `src/limitless/core/shader/` |
| Shader types | `include/limitless/renderer/shader_type.hpp` |
| Lighting | `include/limitless/lighting/lighting.hpp`, `light.hpp`, `light_container.hpp`, `cascade_shadows.hpp`; same under `src/limitless/lighting/` |

All paths above are under **`EngineSource/thirdparty/limitless-engine/`** (with `include/` or `src/` as noted). The **pipeline** namespace is referenced as **`limitless/pipeline/`** (e.g. `deferred/deferred.hpp`, `common/dof_pass.hpp`); that folder was not listed by the file search, but the behavior is inferred from `deferred.cpp` and the renderer Builder.