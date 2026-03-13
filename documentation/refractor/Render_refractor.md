You should implement **visual rendering features in layers**, not randomly.
Each layer unlocks the next one. This is roughly how real engines evolve.

I’ll list them **in the correct dependency order**.

---

# 1️⃣ Core Rendering Foundation (Must exist first)

Before visuals, the renderer must support:

* **Render passes**
* **Render targets / framebuffers**
* **Depth buffer**
* **Pipeline state objects**
* **Shader system**
* **Mesh + vertex buffers**
* **Texture system**
* **Camera system**
* **Material system**

Without this nothing else works.

Result: you can render **basic geometry with shaders**.

---

# 2️⃣ Basic Lighting (First Visual Step)

Implement simple lighting first.

Features:

* Directional light
* Point light
* Spot light
* Lambert / Blinn-Phong

This stage verifies:

* normals
* light calculations
* shadow coordinates
* basic shading pipeline

Do **not start with PBR yet**.

---

# 3️⃣ Shadow System

Lighting is useless without shadows.

Implement:

1. **Shadow map**
2. **PCF filtering**
3. **Directional light shadows**
4. **Point light shadows (cubemap)**
5. **Spot light shadows**

Then improve to:

* **Cascaded Shadow Maps (CSM)**

This is the real standard.

---

# 4️⃣ Physically Based Rendering (PBR)

Now upgrade materials.

Implement:

* **Cook–Torrance BRDF**
* **Metallic / Roughness workflow**
* **Fresnel (Schlick)**
* **Normal maps**
* **AO maps**

Textures:

* Albedo
* Normal
* Roughness
* Metallic
* AO

This gives realistic materials.

---

# 5️⃣ Image Based Lighting (IBL)

PBR without environment lighting looks bad.

Implement:

* HDR skybox
* Irradiance map
* Prefiltered environment map
* BRDF LUT

Now materials respond to environment lighting.

This is the **first AAA visual milestone**.

---

# 6️⃣ Deferred Rendering (or Forward+)

Now restructure lighting for many lights.

Choose one:

* **Deferred renderer**
* **Forward+**
* **Clustered**

Typical pipeline:

GBuffer pass
Lighting pass

GBuffer stores:

* position / depth
* normal
* albedo
* roughness
* metallic

Now you can handle **hundreds of lights**.

---

# 7️⃣ Screen Space Effects

Add realism.

Implement in this order:

1. **SSAO** (ambient occlusion)
2. **SSR** (screen reflections)
3. **SSGI** (optional)
4. **Contact shadows**

These enhance depth perception.

---

# 8️⃣ Post Processing Pipeline

Now add cinematic effects.

Implement:

* HDR rendering
* Tone mapping
* Bloom
* Color grading
* Vignette
* Film grain
* Chromatic aberration

This stage makes visuals **look like a finished game**.

---

# 9️⃣ Transparency & Forward Pass

Transparent objects cannot use deferred.

Implement:

* Forward transparency pass
* Order sorting
* Alpha blending
* Particle rendering

Used for:

* glass
* particles
* UI
* vegetation

---

# 🔟 Advanced Lighting

Now move to modern techniques.

Implement:

* **Clustered lighting**
* **Volumetric lighting**
* **Light shafts**
* **Area lights**

Clustered lighting greatly improves scalability.

---

# 1️⃣1️⃣ Reflection System

Implement:

* Reflection probes
* Box projection
* Probe blending
* Planar reflections (optional)

SSR alone is not enough.

---

# 1️⃣2️⃣ Global Illumination Approximations

Full GI is expensive, so engines approximate.

Options:

* Screen space GI
* Light probes
* Irradiance volumes
* Voxel GI
* DDGI (advanced)

You don't need this immediately.

---

# 1️⃣3️⃣ Advanced Post Effects

These add cinematic quality.

Implement:

* Motion blur
* Depth of field
* Temporal AA (TAA)
* Sharpening filters
* Exposure adaptation

TAA is very important for modern rendering.

---

# 1️⃣4️⃣ Atmospheric Effects

World-scale visuals.

Implement:

* Sky atmosphere
* Volumetric fog
* Height fog
* Clouds

These dramatically improve outdoor scenes.

---

# 1️⃣5️⃣ GPU Driven Rendering

Performance stage.

Implement:

* Frustum culling
* GPU culling
* Multi-draw indirect
* Compute-based visibility
* Instance rendering
* LOD switching

This is **modern engine optimization**.

---

# 1️⃣6️⃣ Streaming Systems

Large worlds require streaming.

Implement:

* texture streaming
* mesh LOD streaming
* async asset loading
* memory budgeting

---

# 1️⃣7️⃣ Optional Next-Gen Features

Only after everything above works.

Examples:

* ray traced shadows
* ray traced reflections
* path tracing mode
* mesh shaders
* virtual shadow maps
* virtual texturing

These are **late-stage features**.

---

# 📊 The True AAA Rendering Ladder

Real engines roughly follow this progression:

```
Geometry Rendering
↓
Basic Lighting
↓
Shadow Maps
↓
PBR
↓
IBL
↓
Deferred / Forward+
↓
SSAO + SSR
↓
Post Processing
↓
Transparency
↓
Clustered Lighting
↓
Reflection Probes
↓
Atmosphere / Fog
↓
TAA / Motion Blur
↓
GPU Driven Rendering
↓
Streaming
↓
Next-gen features
```

---

# ⚠️ Important Reality

Many people try to implement **ray tracing early**.

That is a mistake.

A renderer with:

* PBR
* IBL
* SSAO
* SSR
* TAA
* Bloom
* Atmosphere

already looks **AAA quality**.

---
You are asking the **right question**.
Most engines fail not because of algorithms, but because **features are scattered instead of structured**.

For steps **2–5 (Lighting, Shadows, PBR, IBL)** the key rule is:

> **Lighting is the system. Everything else feeds lighting.**

So these should not be random modules. They should live under a **single lighting architecture**.

Below is a **clean professional structure**.

---

# 1. High Level Renderer Structure

```
render/
│
├─ renderer/
│   ├─ SceneRenderer.cpp
│   ├─ SceneRenderer.h
│
├─ lighting/
│   ├─ LightTypes.h
│   ├─ LightManager.cpp
│   ├─ LightManager.h
│
├─ shadows/
│   ├─ ShadowRenderer.cpp
│   ├─ ShadowRenderer.h
│   ├─ ShadowAtlas.cpp
│   ├─ ShadowAtlas.h
│
├─ materials/
│   ├─ Material.cpp
│   ├─ Material.h
│   ├─ PBRMaterial.cpp
│   ├─ PBRMaterial.h
│
├─ ibl/
│   ├─ IBLSystem.cpp
│   ├─ IBLSystem.h
│   ├─ EnvironmentCapture.cpp
│   ├─ EnvironmentCapture.h
│
└─ shaders/
    ├─ pbr/
    ├─ lighting/
    ├─ shadows/
    ├─ ibl/
```

This structure mirrors how **real engines isolate responsibilities**.

---

# 2. Lighting System (Step 2)

Lighting should be a **central manager**.

### Folder

```
lighting/
```

### Files

```
LightTypes.h
LightManager.h
LightManager.cpp
LightCulling.cpp
```

### Responsibilities

LightTypes:

```
DirectionalLight
PointLight
SpotLight
```

LightManager:

* collects lights from scene
* uploads GPU light buffers
* performs light culling
* provides data for lighting pass

LightCulling:

* tiled / clustered light assignment
* GPU buffers

### Dependency

```
Scene
 ↓
LightManager
 ↓
LightingPass
```

Lighting does **not know about materials**.

---

# 3. Shadow System (Step 3)

Shadows must be **completely separate from lighting**.

Lighting just samples shadow maps.

### Folder

```
shadows/
```

### Files

```
ShadowRenderer.cpp
ShadowRenderer.h

ShadowAtlas.cpp
ShadowAtlas.h

CascadedShadowMaps.cpp
CascadedShadowMaps.h
```

### Responsibilities

ShadowRenderer:

* render shadow maps
* generate shadow matrices
* manage shadow passes

ShadowAtlas:

* pack multiple shadow maps
* manage shadow resolution

CSM module:

* cascade splits
* cascade matrices

### Dependency

```
Scene
 ↓
ShadowRenderer
 ↓
Shadow maps
 ↓
LightingPass
```

Lighting pass only **reads shadow maps**.

---

# 4. PBR Material System (Step 4)

PBR belongs to **materials**, not lighting.

Lighting implements BRDF evaluation.

### Folder

```
materials/
```

### Files

```
Material.h
Material.cpp

PBRMaterial.h
PBRMaterial.cpp

MaterialManager.cpp
MaterialManager.h
```

### Responsibilities

Material:

* generic GPU material data
* textures
* parameters

PBRMaterial:

* albedo
* metallic
* roughness
* normal
* AO

MaterialManager:

* create materials
* upload parameters
* bind descriptor sets

### Shader Side

```
shaders/pbr/
    pbr.glsl
    brdf.glsl
    fresnel.glsl
    lighting_model.glsl
```

Important rule:

Lighting computes BRDF using material data.

Material does **not know about lights**.

---

# 5. Image Based Lighting (Step 5)

IBL is **environment lighting**, not part of materials.

It should be a dedicated system.

### Folder

```
ibl/
```

### Files

```
IBLSystem.cpp
IBLSystem.h

EnvironmentCapture.cpp
EnvironmentCapture.h

BRDFLUTGenerator.cpp
```

### Responsibilities

IBLSystem:

* manages environment maps
* provides irradiance and prefiltered maps
* binds them to lighting pass

EnvironmentCapture:

* generate cubemap from scene
* bake reflection probes

BRDFLUTGenerator:

* generate BRDF integration LUT

### Resources

```
irradianceMap
prefilteredMap
brdfLUT
```

Lighting shader samples these.

---

# 6. Scene Renderer (Where Everything Connects)

```
renderer/
    SceneRenderer.cpp
```

SceneRenderer orchestrates passes.

Example pipeline:

```
Shadow Pass
 ↓
GBuffer Pass
 ↓
Lighting Pass
    ↳ LightManager
    ↳ ShadowRenderer
    ↳ IBLSystem
 ↓
Post Processing
```

SceneRenderer **does not implement algorithms**, it just orchestrates systems.

---

# 7. Dependency Graph (Correct Coupling)

Correct dependency direction:

```
Scene
 ↓
SceneRenderer
 ↓
LightingPass
 ↓
Material + LightManager + ShadowRenderer + IBLSystem
```

Important rule:

```
Lighting depends on materials
Materials do NOT depend on lighting
```

This prevents circular architecture.

---

# 8. Shader Folder Structure

```
shaders/

pbr/
    brdf.glsl
    fresnel.glsl
    distribution.glsl

lighting/
    lighting.glsl
    clustered_lighting.glsl

shadows/
    shadow_sampling.glsl
    shadow_filter.glsl

ibl/
    irradiance.glsl
    reflection.glsl
```

Shaders should be **modular libraries**, not monolithic.

---

# 9. The Clean Mental Model

Think of the renderer like this:

```
Materials describe surfaces
Lights describe energy sources
Shadows block energy
IBL provides ambient energy
Lighting pass evaluates BRDF
```

Each system should reflect this physical model.

---

# 10. What Should NOT Be Coupled

Do NOT do these mistakes:

Lighting → MaterialManager
Material → LightManager
ShadowRenderer → Materials

Correct coupling:

```
Lighting reads:
    lights
    shadow maps
    material parameters
    IBL maps
```

But they **do not know each other internally**.

---

✅ If structured this way:

* renderer stays modular
* Vulkan migration becomes easy
* systems remain replaceable
* complexity stays manageable

---

# OpenEye: Present and Implemented (vs. Refractor Ladder)

Below is what exists in the OpenEye codebase as of the last scan, mapped to the refractor layers. Only **EngineSource** (eyelib, api, app, assets) is considered; third-party libs are excluded.

---

## 1️⃣ Core Rendering Foundation — **Present**

| Item | Location / Notes |
|------|------------------|
| Render passes | `eyelib/render/passes/` (GBufferPass, ShadowPass, LightingPass, TransparencyPass, GpuCullPass, LightCullPass) |
| Render targets / framebuffers | `rhi/RenderTarget*.h/cpp`, `GBuffer.h/cpp`, HDR RT in `RenderPipeline` |
| Depth buffer | RTs with depth (GBuffer, HDR); `BlitDepthOnly` for depth copy |
| Pipeline state | `rhi/PipelineGL`, depth/stencil, cull, blend in passes |
| Shader system | `resources/Shader.h/cpp`, `ShaderCache`, `MaterialCompiler`, `ShaderSourceLoader` |
| Mesh + vertex buffers | `resources/Mesh.h/cpp`, `MeshDescriptor`, Bind/Draw, submeshes |
| Texture system | `Texture2D`, `TextureCube`, `Texture3D`, `TextureArray`, `TextureLoader` |
| Camera system | `CameraView` in `RenderTypes.h`; view/projection, viewPos, near/far |
| Material system | `Material`, `MaterialInstance`, `MaterialBuilder`, UBO (std140), texture bindings |

Result: basic geometry rendering with shaders is supported.

---

## 2️⃣ Basic Lighting — **Present**

| Feature | Implementation |
|---------|----------------|
| Directional light | `LightParams` (direction, color, intensity, ambient, exposure); `SceneLighting.h/cpp`, lighting pass |
| Point light | `PointLight`; up to 8 in `SceneLightingBlock` SSBO; eval in `lighting.frag` |
| Spot light | `SpotLight`; up to 4 in SSBO; eval in `lighting.frag` |
| Lambert / Blinn-Phong–style | Deferred lighting uses diffuse + specular (NdotL, NdotH); PBR-oriented but same pipeline |

---

## 3️⃣ Shadow System — **Present (directional + CSM)**

| Feature | Implementation |
|---------|----------------|
| Shadow map | `CascadeShadowMap.h/cpp`, depth texture array |
| PCF filtering | Shadow sampling in `lighting.frag` (bias, sample) |
| Directional light shadows | Shadow pass from light view; cascade matrices |
| Cascaded Shadow Maps (CSM) | `CascadeShadowMap`, splits, `farBounds`, cascade selection in shader |
| Point / spot light shadows | **Not present** (no cubemap shadow or spot shadow map) |

---

## 4️⃣ PBR — **Present**

| Feature | Implementation |
|---------|----------------|
| Metallic / roughness workflow | MaterialBlock (albedo, metallic, roughness); GBuffer PBR channel; lighting uses them |
| Normal maps | `HAS_NORMAL_MAP`, `u_NormalTexture` in gbuffer/forward shaders |
| AO / AO maps | MaterialBlock `ao`; optional `u_AOTexture` in materials |
| Albedo, roughness, metallic textures | Material textures in GBuffer/forward; `MaterialCompiler` defines |
| Cook–Torrance / Fresnel | `common/brdf.glsl`; lighting uses specular and exposure |

---

## 5️⃣ IBL — **Present**

| Feature | Implementation |
|---------|----------------|
| HDR skybox | `TextureCube`, cubemap skybox pass; procedural fallback (`skybox_procedural`) |
| Irradiance map | `IBL.cpp`: irradiance convolution shader, `BindForLighting` |
| Prefiltered environment map | Prefilter pass; bound for lighting |
| BRDF LUT | BRDF integration LUT generated and bound by IBL |

---

## 6️⃣ Deferred (and Forward / Forward+) — **Present**

| Feature | Implementation |
|---------|----------------|
| Deferred renderer | `GBufferPass` → position, normal, albedo, PBR, velocity, depth; `LightingPass` fullscreen from GBuffer |
| GBuffer contents | Position, normal, albedo, PBR (metallic, roughness, reflective), velocity, depth |
| Forward | Frame graph: Shadow → Forward Opaque → Transparency → Skybox → Post |
| Forward+ (tiled) | `LightCullPass`, `ClusteredLightGrid`, `TiledLightGrid`; legacy path with many lights |

---

## 7️⃣ Screen-Space Effects — **Partially present**

| Feature | Status |
|----------|--------|
| SSAO | **Present**: `PostProcessor` SSAO + blur; `ssao.frag`, `ssao_blur.frag` |
| SSR | **Present**: `PostProcessor` SSR; `ssr.frag`; uses G-buffer + reflective strength |
| SSGI | **Not present** |
| Contact shadows | **Not present** |

---

## 8️⃣ Post-Processing Pipeline — **Present**

| Feature | Implementation |
|---------|----------------|
| HDR rendering | HDR render target; tone mapping to LDR |
| Tone mapping | `PostProcessor` (ACES-style option), `tonemap.frag` |
| Bloom | Bloom extract, blur, composite; `PostProcessor::Settings` |
| FXAA | `fxaa.frag`, enable in PostProcessor |
| Vignette | `vignette.frag` |
| Film grain | `film_grain.frag` |
| Chromatic aberration | `chromatic_aberration.frag` |
| Color grading | **Not present** as a dedicated pass |

---

## 9️⃣ Transparency & Forward Pass — **Present**

| Feature | Implementation |
|---------|----------------|
| Forward transparency pass | `TransparencyPass`; sorted transparent drawables over HDR |
| Alpha blending | Material `Blending::Translucent`, blend state in passes |
| Particle rendering | Particle shaders (`particle.vert/frag`, `particle_volumetric.frag`) |
| Order sorting | Transparency pass uses drawable list (sorting by pipeline) |

---

## 🔟 Advanced Lighting — **Partially present**

| Feature | Status |
|----------|--------|
| Clustered / tiled lighting | **Present**: `LightCullPass`, `ClusteredLightGrid`, `TiledLightGrid` for Forward+ |
| Volumetric lighting / light shafts | **Not present** |
| Area lights | **Not present** |

---

## 1️⃣1️⃣ Reflection System — **Partial (SSR only)**

| Feature | Status |
|----------|--------|
| SSR | **Present** (see 7️⃣) |
| Reflection probes / box projection / blending | **Not present** |
| Planar reflections | **Not present** |

---

## 1️⃣2️⃣–1️⃣4️⃣ GI, Advanced Post, Atmosphere — **Mostly not present**

| Layer | Status |
|-------|--------|
| GI approximations (SSGI, light probes, voxel, DDGI) | **Not present** |
| Motion blur | **Present**: `PostProcessor` + `motion_blur.frag` |
| DoF, TAA, sharpening, exposure adaptation | **Not present** |
| Sky atmosphere, volumetric fog, height fog, clouds | **Not present** (only procedural skybox) |

---

## 1️⃣5️⃣ GPU-Driven Rendering — **Partially present**

| Feature | Implementation |
|---------|----------------|
| GPU culling | `GpuCullPass`; compute cull; indirect draw list |
| Multi-draw indirect | `MultiDrawIndexedIndirect` in GBuffer/Shadow when using GPU cull |
| Instance rendering | `gbuffer_instanced`, `shadow_instanced`; instanced draw path |
| LOD | `LODManager.h/cpp`; LOD selection in scene/build path |
| Frustum culling | CPU-side in scene/prepare; GPU cull in GpuCullPass |

---

## 1️⃣6️⃣–1️⃣7️⃣ Streaming and Next-Gen — **Hooks / not present**

| Feature | Status |
|----------|--------|
| Texture/mesh streaming, async load, memory budget | `StreamingHooks`; no full streaming system |
| Ray tracing, mesh shaders, virtual shadow maps, virtual texturing | **Not present** |

---

## Summary Table

| Refractor layer | OpenEye status |
|-----------------|----------------|
| 1 Core foundation | ✅ Present |
| 2 Basic lighting | ✅ Present (dir + point + spot) |
| 3 Shadows | ✅ Directional + CSM; ❌ point/spot shadows |
| 4 PBR | ✅ Present |
| 5 IBL | ✅ Present |
| 6 Deferred / Forward+ | ✅ Present |
| 7 Screen space | ✅ SSAO, SSR; ❌ SSGI, contact shadows |
| 8 Post | ✅ Tone map, bloom, FXAA, vignette, film grain, chroma, motion blur; ❌ color grading |
| 9 Transparency | ✅ Present |
| 10 Advanced lighting | ✅ Tiled/clustered; ❌ volumetric, area lights |
| 11 Reflection | ✅ SSR only; ❌ probes, planar |
| 12–14 GI, advanced post, atmosphere | ❌ Except motion blur |
| 15 GPU-driven | ✅ GPU cull, indirect draw, instancing, LOD |
| 16–17 Streaming, next-gen | ❌ Only hooks |

---
