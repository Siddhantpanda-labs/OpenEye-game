Your reasoning is basically correct. What you are seeing is a **device-availability timing bug introduced by the descriptor-only refactor**. Let me compress the architecture lesson and the real fix.

---

# What Actually Broke

Before the refactor:

```
BindAssets
   ↓
Load mesh/material
   ↓
Create GPU resources immediately
   ↓
Store in cache
```

After the refactor:

```
BindAssets
   ↓
Store descriptor only
   ↓
First GetMesh/GetMaterial
   ↓
Create GPU resource IF device exists
```

But your new flow allows this:

```
BindAssets
   ↓
BuildScene
   ↓
GetMesh(id)
   ↓
device == null
   ↓
return cache lookup
   ↓
cache empty
   ↓
null mesh
   ↓
nothing rendered
```

So **nothing is actually created**, which is why meshes disappear.

---

# The Root Architectural Problem

You introduced **lazy GPU creation** but did not enforce the invariant:

> GPU resource creation requires a valid device.

Right now the system silently fails when that invariant is broken.

Professional engines **never allow that silent path**.

---

# The Real Fix (Architectural)

You must enforce this rule:

> The render device must be registered before any asset resolution occurs.

Concretely:

```
CreateDevice
      ↓
AssetRegistry.SetRenderDevice(device)
      ↓
BindAssets
      ↓
BuildScene
      ↓
Render
```

If `GetMesh` or `GetMaterial` runs without a device, it should **never return null silently**.

---

# Critical Change to Make

Inside `AssetManager::GetMesh` / `GetMaterial`:

Instead of this logic:

```
if (device == null)
    return cache.Get(id)
```

Do this:

```
ASSERT(device && "Render device not set before asset resolution");
```

This forces initialization order correctness.

Silent fallback is what created your invisible meshes.

---

# A More Robust Pattern (Recommended)

Introduce a **device readiness gate**.

Example conceptually:

```
AssetRegistry
    descriptors
    device
    ready
```

Flow:

```
BindAssets
      ↓
Registry.ready = false
      ↓
SetRenderDevice(device)
      ↓
UploadPendingAssets()
      ↓
Registry.ready = true
```

Now:

```
GetMesh
   assert(Registry.ready)
```

No race. No null returns.

**Applied:** Device readiness gate implemented. `AssetManager` has `m_readyForResolution`: set true in `SetRenderDevice(device)` when device is non-null, set false in `ClearDescriptorsAndInstances()` and `OnDeviceLost()`. `GetShader` / `GetMesh` / `GetTexture` / `GetMaterial` assert `m_readyForResolution && m_renderDevice` before resolving. `api::AssetRegistry::IsReadyForResolution()` forwards to the manager. No separate `UploadPendingAssets()` step; ready is set as soon as device is set.

---

# Builtin Primitives Check

You also asked about builtins earlier.

They follow a different path:

```
GetPrimitiveEntry
   ↓
EnsureDefaultMaterial
```

If builtins are invisible too, check:

```
default shader loaded?
device set before EnsureDefaultMaterial?
```

But based on your description, this is likely only affecting **file-based assets**, not builtins.

**Applied:** Debug assert when default or skinned shader fails to load for primitives. In `BuiltinResourceManager::EnsureDefaultMaterial` / `EnsureSkinnedMaterial` and `WorldRenderBridge::EnsureDefaultMaterial` / `EnsureSkinnedMaterial` (fallback path when no BuiltinResourceManager), if we fall through to empty material or null skinned shader, we now `assert(false && "...")` in debug builds (`#ifndef NDEBUG`) so init-order or missing shader is caught instead of silent invisible primitives.

---

# Minimal Fix (Quick)

Make sure this happens before scene building:

```
device = renderPipeline.GetDevice()

assetRegistry.SetRenderDevice(device)
```

And add assertions inside:

```
GetMesh
GetMaterial
```

**Applied:** (1) In `AssetManager::GetShader` / `GetMesh` / `GetTexture` / `GetMaterial`, the silent `if (!device) return cache.Get(id)` was removed and replaced with `assert(m_renderDevice && "Render device not set before asset resolution ...")` so init-order bugs fail fast instead of returning null. (2) In `GameRuntime::Initialize`, after `m_render->Initialize()` we now call `assetRegistry.SetRenderDevice(m_render->GetDevice())` and `m_render->InitializeDefaultResources()` so the registry has a device before any `BuildScene` (and builtin default materials exist). PlayerApp already had the correct order (SetRenderDevice before BindAssets).

---

# Long-Term Clean Architecture

Professional engines split responsibilities:

```
AssetRegistry (CPU)
    descriptors

Renderer
    GPU resources

AssetResolver
    converts descriptor → GPU resource
```

So scene building never touches the registry directly.

But that’s a bigger step.

---

## Design compliance (current codebase vs long-term)

**Followed**


| Rule                                                     | Status  | Where                                                                                                                                                                                                                                                                                                                 |
| -------------------------------------------------------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| World/ECS does not know about GPU resources              | **Yes** | World stores `meshAssetId` / `materialAssetId` (handles); no GPU pointers in ECS.                                                                                                                                                                                                                                     |
| Asset system has CPU descriptors                         | **Yes** | `MeshDescriptor`, `TextureDescriptor`, `ShaderDescriptor`, `MaterialDescriptor`; `LoadAssetDescriptorOnly` stores descriptors only.                                                                                                                                                                                   |
| GPU creation from descriptor on first use                | **Yes** | `AssetManager::Get`* or `IGPUResourceResolver::Get*` create from descriptor on cache miss; device + readiness asserted.                                                                                                                                                                                               |
| Scene Builder does not create GPU resources itself       | **Yes** | `BuildScene` / `BuildSceneFromViews` request materials via bridge/resolver; resolution (descriptor → GPU) is in AssetManager or renderer-owned resolver.                                                                                                                                                              |
| GPU creation/cache in renderer when resolver set         | **Yes** | Player: `WorldRenderBridge` owns `RendererGPUResourceCache` (implements `IGPUResourceResolver`); `AssetManager::SetGPUResolver` delegates Get* to it. Cache lives in bridge; descriptors in AssetManager. Editor: no resolver → AssetManager built-in cache.                                                          |
| Scene building never touches the registry for resolution | **Yes** | Player: `WorldRenderBridge::ResolveMaterial(id)` / `ResolveMaterial(path, registry)`; registry used only for `GetId(path)`. Editor: `EditorSceneBuilder::ResolveMaterialForScene(registry, path)` uses registry only for GetId, then AssetManager for GPU resolution. No `registry->GetMaterial` in scene build path. |
| RHI abstracts the API                                    | **Yes** | `rhi::IDevice`, buffers, pipelines, command lists; no direct OpenGL in scene/asset code.                                                                                                                                                                                                                              |
| Renderer Core owns pipeline and passes                   | **Yes** | `WorldRenderBridge` / `RenderPipeline` own device and passes (GBuffer, Shadow, Lighting, etc.).                                                                                                                                                                                                                       |


**Partial / not yet**


| Rule                                            | Status      | Where                                                                                                                                                                                                 |
| ----------------------------------------------- | ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| CPU assets and GPU resources are separate types | **Partial** | Descriptors are the canonical CPU form; `Mesh`/`Material`/`Shader`/`Texture` still hold both CPU state (geometry, reload params, assetId) and GPU handles. See *Target: CPU vs GPU type split* below. |
| No global device usage                          | **Partial** | Pipeline and passes pass device explicitly; `Mesh.h`/`Shader.h` document SetDevice as legacy. Prefer Bind(device)/Draw(device) in new code.                                                           |
| Render Graph                                    | **No**      | Pass order and dependencies are manual (pipeline code). Future design outlined in "Render Graph (future)" in Implementation tracking.                                                                 |


**Target: CPU vs GPU type split**

- **Canonical CPU assets:** `MeshDescriptor`, `ShaderDescriptor`, `TextureDescriptor`, `MaterialDescriptor` (plus `AssetId` in the registry). Stored and loaded via `LoadAssetDescriptorOnly`; no GPU pointers.
- **GPU instances (current):** `Mesh`, `Shader`, `Texture2D`, `Material` — used at draw time. They currently still hold:
  - **Mesh:** CPU geometry (vertices, indices, submeshes, AABB) for `CreateFromDescriptor`/editing and `GetDescriptor()`; GPU: device, vertex/index buffers.
  - **Shader:** CPU reload params (last paths, layout, topology, keywords); GPU: device, pipeline.
  - **Texture2D:** CPU size/format and optional assetId; GPU: device, texture handle.
  - **Material:** PBR params and refs to Shader/Texture (GPU instances); no separate MaterialDescriptor at draw time (descriptor used only for creation).
- **Target end state:** GPU types hold only device + RHI handles (and minimal draw-time info such as submesh ranges). All identity and source data live in descriptors; creation is “from descriptor → GPU handle only.” Next steps: (1) Mesh: keep descriptor as sole owner of vertex/index data; Mesh becomes device + buffers + submesh metadata only (geometry passed in at `CreateFromDescriptor`, not retained). (2) Shader: move reload state into a separate descriptor or loader; Shader holds only device + pipeline. (3) Texture2D/Material: ensure no unnecessary CPU state retained. (4) Optional: introduce explicit GPU-handle types (e.g. `MeshHandle`) used in the draw path, with Mesh remaining the “full” type for load/edit until migration is complete.

**Summary:** Descriptor-only binding, readiness gate, and resolve-on-first-Get* match the intended flow. **Applied:** Scene building uses registry only for GetId; player resolution goes through `WorldRenderBridge::ResolveMaterial` → `AssetManager::GetMaterial` → `RendererGPUResourceCache` (owned by bridge). Editor uses `EditorSceneBuilder::ResolveMaterialForScene` with AssetManager’s built-in path (no resolver). Remaining gaps: (1) Mesh/Material/Shader/Texture are still combined CPU+GPU types; (2) no Render Graph.

---

# The Key Rule Going Forward

Never allow this state:

```
GPU resource requested
device not available
system silently returns null
```

Instead enforce:

```
device must exist
or crash immediately
```

That single rule prevents **an entire class of invisible rendering bugs**.

---

The deeper structural flaw is this:

> **Your CPU assets and GPU resources are the same object.**

Right now objects like `**Mesh`**, `**Material**`, `**Shader**`, `**Texture**` likely contain both:

- asset identity / metadata
- GPU handles
- device pointer
- pipeline / buffers

Conceptually something like:

```cpp
class Shader {
    std::string path;     // asset identity
    Device* device;       // GPU dependency
    PipelineHandle pipe;  // GPU object
};
```

This works in small systems, but it creates **three major long-term problems**.

---

# 1️⃣ Hot-Reload Breaks

If a shader file changes:

```
basic.frag modified
```

What you want:

```
reload shader source
recompile pipeline
swap GPU pipeline
materials keep working
```

But with the current design:

```
Shader object == GPU pipeline
```

So reload becomes:

```
destroy object
create new object
update every reference
```

This becomes extremely fragile in large scenes.

Professional engines separate:

```
ShaderAsset (CPU)
    ↓
ShaderGPUInstance (device specific)
```

Hot reload only rebuilds the **GPU instance**, not the asset.

---

# 2️⃣ Multi-Device / Multi-Window Breaks

Right now a shader contains a **single device pointer**.

But imagine:

```
Editor window
Game window
VR mirror window
Headless training renderer
```

You may need **multiple devices or contexts**.

With the current design:

```
Shader → Device A
```

You cannot create the same shader for Device B.

Correct architecture:

```
ShaderAsset
   ↓
GPUInstance(device A)
GPUInstance(device B)
```

Same asset, different GPU representations.

---

# 3️⃣ Device Loss / Rebuild Becomes Impossible

Modern engines must handle:

- GPU driver reset
- swapchain recreation
- resolution change
- backend switch

To recover, the engine must rebuild GPU objects.

But if the asset **is the GPU object**, you cannot rebuild safely.

Instead the system should do:

```
ShaderAsset
    source files
    metadata

Device lost
    ↓
recreate GPU pipelines from ShaderAsset
```

---

# What the Correct Architecture Looks Like

Split every GPU-dependent asset into **two layers**.

### CPU Layer (device-independent)

```
MeshAsset
MaterialAsset
ShaderAsset
TextureAsset
```

Stored in the **AssetRegistry**.

Example:

```cpp
struct MeshAsset {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
```

---

### GPU Layer (device-dependent)

```
MeshGPU
MaterialGPU
ShaderGPU
TextureGPU
```

Created by the renderer.

Example:

```cpp
struct MeshGPU {
    Buffer vertexBuffer;
    Buffer indexBuffer;
};
```

---

# Correct Data Flow

```
AssetRegistry
   ↓
SceneBuilder
   ↓
Renderer
   ↓
Create GPU instance if needed
   ↓
Cache per device
```

So the renderer owns GPU resources.

Assets remain pure CPU data.

---

# What This Fixes

Once separated, you get automatically:

✅ hot-reloadable shaders
✅ multi-device rendering
✅ device loss recovery
✅ background asset streaming
✅ editor/runtime coexistence
✅ deterministic lifetime

---

# Why Your Refactor Exposed This

Your descriptor-only refactor moved toward this direction:

```
descriptor
↓
lazy GPU creation
```

But the underlying asset types are still **GPU-bound objects**, so the architecture is halfway between both worlds.

That’s why subtle bugs appear.

---

# The Core Rule Professional Engines Follow

> **Assets are device-independent.
> GPU resources are device-dependent.**

Never mix them in the same object.

---

# The Good News

You already started moving toward the correct architecture with:

```
descriptor binding
lazy creation
```

You just need to **complete the separation**.

---

Great. Here is a **clean architecture diagram of a modern rendering engine**. This is the structure most professional engines converge toward (Unreal-style, Frostbite-style, modern custom engines).

I’ll keep it **simple, hierarchical, and practical** so you can map it to your engine.

---

# Modern Rendering Engine Architecture

```
Application
│
├── World / ECS
│     Entities, Components, Transforms, Cameras
│
├── Scene Builder
│     Converts world state → RenderScene
│
├── Asset System (CPU domain)
│     MeshAsset
│     MaterialAsset
│     TextureAsset
│     ShaderAsset
│
├── Renderer Core
│     Resource Manager
│     GPU Resource Cache
│
├── Render Graph / Frame Graph
│     Pass scheduling
│     dependency resolution
│
├── Render Pipelines
│     Shadow pass
│     GBuffer pass
│     Lighting pass
│     Postprocess
│
├── RHI (Render Hardware Interface)
│     Device
│     Buffers
│     Pipelines
│     Command lists
│
└── GPU / Driver
```

Each subsystem has a **very strict responsibility**.

---

# 1. World / ECS

This is the **game simulation layer**.

Example components:

```
Transform
MeshRenderer
Camera
Light
```

Example entity:

```
Entity
 ├ Transform
 ├ MeshRenderer(mesh=Cube, material=Metal)
```

This layer **does not know anything about GPU resources**.

It only references **asset handles**.

---

# 2. Scene Builder

This layer converts ECS data into a **renderable scene snapshot**.

```
World
   ↓
SceneBuilder
   ↓
RenderScene
```

Example output:

```
RenderScene
 ├ RenderObjects
 ├ Lights
 ├ Camera
 ├ Sky
```

Important rule:

> SceneBuilder never creates GPU resources.

It only prepares render data.

---

# 3. Asset System (CPU Only)

This system stores **asset definitions**.

```
MeshAsset
MaterialAsset
TextureAsset
ShaderAsset
```

Example:

```
MeshAsset
 ├ vertices
 ├ indices
```

```
ShaderAsset
 ├ vertex shader path
 ├ fragment shader path
```

The asset system never touches the GPU.

---

# 4. Renderer Core

This is where **GPU resources are managed**.

Responsibilities:

```
CreateMeshGPU
CreateTextureGPU
CreateShaderPipeline
```

It maintains caches like:

```
meshResources[MeshHandle]
textureResources[TextureHandle]
shaderResources[ShaderHandle]
```

Lazy creation happens here.

---

# 5. GPU Resource Cache

Maps CPU assets → GPU objects.

Example:

```
MeshHandle → MeshResource
TextureHandle → TextureResource
ShaderHandle → ShaderResource
```

Example GPU resource:

```
MeshResource
 ├ vertexBuffer
 ├ indexBuffer
```

These are device-specific.

---

# 6. Render Graph (Frame Graph)

Modern engines schedule rendering through a **frame graph**.

Example passes:

```
ShadowPass
GBufferPass
LightingPass
PostProcessPass
UI
```

Graph example:

```
ShadowMap
    ↓
GBuffer
    ↓
Lighting
    ↓
HDR
    ↓
Post
```

The graph manages:

• resource lifetimes
• pass order
• dependencies

This removes many manual pipeline bugs.

---

# 7. Render Pipelines

Each pass performs specific rendering.

Example:

```
Shadow Pipeline
GBuffer Pipeline
Lighting Pipeline
Forward Pipeline
PostFX Pipeline
```

Each pipeline uses GPU resources created by the renderer core.

---

# 8. RHI (Render Hardware Interface)

This layer abstracts the graphics API.

```
Device
CommandBuffer
Pipeline
Buffer
Texture
Framebuffer
```

Backends may be:

```
OpenGL
Vulkan
DirectX
Metal
```

Higher systems **never talk to OpenGL/Vulkan directly**.

---

# Complete Data Flow

Here is the **entire rendering flow**.

```
World (ECS)
     ↓
SceneBuilder
     ↓
RenderScene
     ↓
Renderer Core
     ↓
GPU Resource Cache
     ↓
Render Graph
     ↓
Render Pipelines
     ↓
RHI
     ↓
GPU
```

Every stage has a **single clear responsibility**.

---

# How This Relates To Your Current Engine

Your engine already contains many of these pieces.

Rough mapping:

```
World / ECS
   → ctx->world

Scene Builder
   → EditorSceneBuilder / BuildScene

Asset System
   → AssetRegistry

Renderer Core
   → WorldRenderBridge

Render Pipelines
   → RenderPipeline

RHI
   → rhi::Device
```

You are **about 60-70% of the way toward a professional architecture**.

The biggest remaining improvements are:

1. **Separate CPU assets from GPU resources**
2. **Move GPU creation fully into the renderer**
3. **Remove global device usage**
4. **Introduce a render graph eventually**

---

# Key Rule to Remember

The architecture works because of this rule:

> Simulation systems describe *what to render*.
> Renderer systems decide *how to render it*.

Never mix those.

---

# Implementation tracking

**Done**


| Item                                         | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| -------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Device readiness gate                        | `AssetManager::m_readyForResolution`; Get* assert before resolve.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| SetRenderDevice before resolution            | Player/Editor set device once; GameRuntime order: Initialize → SetRenderDevice → InitializeDefaultResources → BindAssets/BuildScene.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| BindAssets CPU-only                          | `LoadAssetDescriptorOnly` in binding; GPU creation on first Get* when device ready.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Renderer-owned GPU cache (player)            | `RendererGPUResourceCache` + `SetGPUResolver`; bridge owns cache; registry descriptors only.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Scene building uses resolver                 | Player: `ResolveMaterial(id)`; Editor: `ResolveMaterialForScene(registry, path)`; registry only for GetId.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Explicit device in pipeline                  | `Mesh::Bind(device)`, `Mesh::Draw(device)`, `Shader::Bind(device)`; RenderPipeline and passes pass `m_device` / `ctx.device`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Single device (Model A)                      | Editor + Play share one device; no cross-lifetime GPU refs in registry.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Target: CPU vs GPU types                     | Documented in "Target: CPU vs GPU type split"; next steps listed.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| RenderContext for passes                     | GBuffer, Shadow, Lighting, Transparency, GpuCullPass already have ctx-based API; LightCullPass now has `Execute(ctx)`; pipeline calls ctx-based entry points.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Device-loss handling                         | `AssetManager::OnDeviceLost()` clears instance caches and notifies resolver; descriptors kept. `api::AssetRegistry::OnDeviceLost()` forwards to manager. App sequence: (1) assetRegistry.OnDeviceLost(), (2) pipeline->Shutdown(), (3) destroy device, (4) create new device, (5) SetRenderDevice(newDevice), (6) pipeline->Initialize(newDevice, w, h).                                                                                                                                                                                                                                                                                                                                                                                |
| Explicit device preferred (API)              | Pipeline uses `Bind(device)` / `Draw(device)`; `Mesh.h` and `Shader.h` document `SetDevice` as legacy; new code should pass device explicitly.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| Remove SetDevice/GetDevice (explicit device) | **Done.** Core: `Mesh`/`Shader`/`Material`/passes use `Bind(device)`/`Draw(device)`/ctx. IBL, DebugDrawRenderer, BatchRenderer, InstancedRenderer, Renderer, Primitives, SpriteRenderer, TextRenderer: device passed explicitly or via `SetDevice`. Geometry/animation: no `UploadToGPU()` at call sites; `Mesh::Bind(device)` does lazy upload. API: `api::Mesh::UploadToGPU(device)`, `api::Shader::Bind(device)`/`BuildFromSource(device,...)`/`LoadFromFile(device,...)`. API passes (GBuffer, Shadow, Lighting): `SetDevice(device)` + stored `m_device` used to build `RenderContext` when calling internal pass. Callers of API passes or Batch/Instanced renderers must call `SetDevice(device)` before first `Render`/`Flush`. |
| CPU vs GPU type split (Phase 2)               | Steps 2.1–2.6 done. Phase 2.4 (strip Mesh of CPU geometry) complete: all geometry ops and loaders use EditableMesh/descriptor; ParticleRenderer builds via MeshDescriptor and Mesh::Create(device, desc). |


**In progress**


| Item | Notes |
| ---- | ----- |
| (none) | — |


**To do (future / phased)**


| Item         | Notes                                                                           |
| ------------ | ------------------------------------------------------------------------------- |
| Render Graph | Pass order and dependencies via frame graph; see "Render Graph (future)" below. |


---

## Phase 2: CPU vs GPU split

**Goal:** Separate device-independent asset data (CPU) from device-bound GPU resources. Enables hot-reload, multi-device, and clean device-loss recovery.

**Planned steps:**


| Step | Description                                                                                                                                                                                                                                                                                                                                                      | Status |
| ---- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------ |
| 2.1  | Introduce `MeshHandle`: holds only vertex buffer, index buffer, index type, total index count, submesh metadata. No CPU geometry. `Mesh::GetHandle()` returns handle when uploaded; `MeshHandle::Bind(device)`, `MeshHandle::Draw(device, submeshIndex)`. `Mesh::Draw` delegates to handle after lazy upload.                                                    | Done   |
| 2.2  | `Mesh` keeps CPU geometry (vertices, indices, submeshes) for editing/loading; `UploadToGPU(device)` creates/updates internal GPU buffers. `Mesh::GetHandle()` produces a `MeshHandle` for the draw path.                                                                                                                                                         | Done   |
| 2.3  | Migrate draw path to use `MeshHandle` where possible (passes, Drawable). GBufferPass, ShadowPass, TransparencyPass, RenderPipeline (deferred, forward, entity ID), BatchRenderer, InstancedRenderer now get `MeshHandle h = mesh->GetHandle()` after `mesh->Bind(device)` and call `h.Draw(device)` when valid. `Mesh::Bind`/`Draw` unchanged for other call sit | Done   |
| 2.4  | es.(Optional) Strip `Mesh` of CPU geometry: Mesh becomes GPU-only (same as MeshHandle); all geometry lives in `MeshDescriptor` / `EditableMesh`. Load/edit code uses descriptor; `CreateFromDescriptor(device, desc)` returns Mesh (GPU-only). Large change; many GetVertices/SetVertices call sites.                                                           | Done        |
| 2.5  | Shader: reload state in `ShaderDescriptor` only. Shader holds `m_descriptor` (reload identity) + device + pipeline + reflection. `Load`/`LoadFromFile` set `m_descriptor`; `GetDescriptor()` returns it; `Reload()` calls `LoadFromDescriptor(device, m_descriptor)`.                                                                                            | Done   |
| 2.6  | Texture2D / Material: ensure no unnecessary CPU state retained; descriptor is source of truth.                                                                                                                                                                                                                                                                   | Done   |


**Texture2D (2.6):** Reload state is now in `TextureDescriptor m_descriptor` only. Removed `m_lastFilePath`, `m_lastIsSRGB`, `m_lastGenerateMipmaps`. `LoadFromFile` sets `m_descriptor`; `GetDescriptor()` returns it; reload path is `LoadFromDescriptor(device, m_descriptor)`.

**Material:** No separate descriptor type; shader + texture refs + PBR/custom params are the identity. No duplicate "last loaded" state; no change needed.

**Phase 2.4 (done):**
- **Done:** `Mesh` is GPU-only (no `SetVertices`/`GetVertices`/`UploadToGPU`). `MeshDescriptorUtils`: `GenerateNormals`, `GenerateTangents`, `Optimize`, `ComputeAABB`. `Mesh::Create(device, desc)`, `CreateFromDescriptor(device, desc)`. `EditableMesh::GetDescriptor()`, `BuildRenderMesh(out, device)`, `CopyToRenderMesh(out, device)`, `FromDescriptor(desc)`. API `Mesh` holds `EditableMesh` (CPU) + `render::Mesh` (GPU); `GetEditableMesh()`, `UploadToGPU(device)` syncs. FullscreenQuad, IBL, RenderPipeline quad, FluidFoamRenderer, FluidSurfaceExtractor, MeshLoader use descriptor/create path. Geometry ops (MeshSubdivide, MeshBevel, MeshDecimate, MeshDisplace, MeshSmooth, MeshColorPaint, MeshNoise, MeshExtrude, MeshTexturePaint, MeshDeform, MeshBoolean), RiggingUtils, Mask, MeshSnapshot, MeshPrimitives, SelectionBuilder, ModelLoader, MeshBinaryFormat, and **ParticleRenderer** all use `EditableMesh` or descriptor; API layer passes `mesh.GetEditableMesh()` or builds via descriptor. ParticleRenderer (eyelib) builds `MeshDescriptor` and returns `Mesh::Create(device, desc)`; API ParticleRenderer fills api::Mesh (EditableMesh) and sets one submesh.

---

## Render Graph (future)

- **Goal**: Encode pass order and resource dependencies in a frame graph; compile to a command list or execute list each frame.
- **Nodes**: One node per pass (GBuffer, Shadow, Lighting, PostProcess, etc.).
- **Edges**: Resource dependencies (e.g. Lighting reads GBuffer outputs; PostProcess reads HDR target).
- **Benefits**: Clear resource lifecycle, automatic barriers, optional async compile, easier to add/remove passes.
- **Scope**: Out of current refactor scope; document here for when threading and advanced scheduling are in focus.

