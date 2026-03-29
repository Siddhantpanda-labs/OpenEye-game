# Render Module — Structure & Architecture

This document describes the structure and architecture of the render module: **eyelib/render** (core) and **api/render** (public API layer).

---

## 1. Overview

The render module provides:

- **RHI (Render Hardware Interface)** — backend-agnostic abstraction over OpenGL and optional Diligent Engine.
- **Scene model** — drawables, camera, lights, skybox; consumed via a read-only view interface.
- **Frame graph** — declarative pass ordering with DAG-based execution and transient texture aliasing.
- **Pipeline modes** — Deferred (GBuffer + lighting), Forward (frame-graph), and Forward+ (legacy tiled).
- **Resources** — GPU-only mesh, shaders, materials, textures (2D, 3D, cube, array); CPU data lives in descriptors / `EditableMesh`.

Design principles:

- **Explicit device passing** — RHI and resource creation/load APIs take `rhi::IDevice*` (or `api::RenderDevice` at API layer); no global or injected device for render resources.
- **Zero-copy scene view** — Pipeline renders from `IRenderSceneView`; API and ECS both implement it over their scene data without copying.
- **CPU vs GPU split** — Mesh is GPU-only; CPU geometry lives in `MeshDescriptor` / `EditableMesh`. Textures load/create with an explicit device.

---

## 2. Directory Layout

### 2.1 eyelib/render (core)

```
eyelib/render/
├── rhi/                    # RHI interface and backends
│   ├── RHI.h               # IDevice, ICommandList, IBuffer, ITexture, IShader, IPipeline, etc.
│   ├── RHITypes.h          # BufferDesc, TextureDesc, VertexLayout, enums (ResourceState, etc.)
│   ├── CreateDevice.h/cpp  # CreateOpenGLDevice(), CreateGraphicsDevice(), CreateDiligentDevice()
│   ├── RHIUtils.cpp        # Helpers (e.g. ClearAndSetRenderTargetWithViewport)
│   ├── OpenGL/             # OpenGL backend (DeviceGL, CommandListGL, BufferGL, TextureGL, ...)
│   └── diligent/           # Diligent Engine backend (optional, when OPENEYE_DILIGENT_AVAILABLE)
├── framegraph/
│   ├── FrameGraph.h/cpp    # DAG-based frame graph: CreateTexture, ImportTextureRHI, AddPass, Compile(device), Execute
│   └── FrameGraphRecipe.h/cpp  # Declarative pass list (name, reads, writes) + AddPassesFromRecipe()
├── passes/
│   ├── GBufferPassSystem   # Deferred geometry system: fill position, normal, albedo, PBR, velocity, depth
│   ├── ShadowPass          # Cascade directional shadow map
│   ├── LightingPass        # Full-screen lighting from GBuffer (+ shadow); reads scene lighting from SSBO (binding 5) when provided
│   ├── TransparencyPass    # Sorted transparent drawables
│   ├── GpuCullPass         # GPU culling / indirect draw list
│   └── LightCullPass       # Clustered/tiled light culling (Forward+)
├── resources/
│   ├── Mesh.h/cpp          # GPU-only mesh; CreateFromDescriptor(device, desc), Bind/Draw(device)
│   ├── Shader.h/cpp        # GPU shader/pipeline; Load(device, name, layout, topology)
│   ├── Material.h/cpp      # Material parameters + texture bindings
│   ├── MaterialInstance.h/cpp
│   ├── Texture2D.h/cpp     # 2D texture; Create(device, ...), LoadFromFile(device, ...)
│   ├── Texture3D.h/cpp
│   ├── TextureCube.h/cpp   # Cubemap; LoadFromFiles(device, ...) — device required
│   ├── TextureArray.h/cpp
│   ├── TextureLoader       # LoadPNG/JPEG/HDR(device, path), CreateFromDecoded(device, ...)
│   └── TextureTypes.h
├── IRenderSceneView.h      # Read-only scene view: GetCamera, GetLight, GetSkybox, ForEach*Drawable/Light
├── RenderTypes.h           # Drawable, CameraView, LightParams, PointLight, SpotLight, RenderScene
├── RenderContext.h         # { device, cmd } for pass invocation
├── RenderPipeline.h/cpp    # Orchestrator: Init(device), Render(view), frame graph build/execute
├── GBuffer.h/cpp           # Deferred G-buffer RT and accessors
├── CascadeShadowMap.h/cpp
├── SceneLighting.h/cpp     # Phase 5: scene lighting SSBO (directional + point + spot); FillSceneLightingBuffer; bind in LightingPass
├── PostProcessor.h/cpp     # Tone mapping, bloom, FXAA, SSAO, SSR, etc.
├── FullscreenQuad.h/cpp
├── MeshDescriptor.h        # CPU mesh data (vertices, indices, submeshes)
├── MeshDescriptorUtils.h   # GenerateNormals, GenerateTangents, Optimize, ComputeAABB
├── ShaderDescriptor.h / ShaderReflection.h
├── TextureDescriptor.h
├── ClusteredLightGrid, GpuProfilerRHI, PipelineCache, StreamingHooks, ...
├── Renderer.h/cpp          # Batch / immediate-style draw (eyelib)
├── 2d/                     # Sprite, Font, TextRenderer, SpriteRenderer, Primitives
└── ARCHITECTURE.md         # This file
```

### 2.2 api/render (public API)

- Thin wrappers around eyelib types: **RenderPipeline**, **RenderScene**, **Renderer**, **Mesh**, **Material**, **Shader**, **Texture**, **TextureCube**, **Texture3D**, **TextureArray**, **GBuffer**, **CascadeShadowMap**, **FrameGraph**, **PostProcessor**, **IBL**, **RenderTarget**, **Camera**, **Light**, **LODManager**, etc.
- **RenderSceneAdapter**: `api::RenderScene` → `ApiRenderSceneView` implementing `eyelib::render::IRenderSceneView` (zero-copy: iterates API scene storage).
- **RenderDevice**: opaque handle (`void*`) for passing the RHI device from pipeline to asset loaders and other systems.
- **FrameGraph**: API `Compile(RenderDevice device)` forwards to eyelib `Compile(rhi::IDevice*)`; no parameterless overload.

---

## 3. RHI (Render Hardware Interface)

### 3.1 Interface (rhi/RHI.h, RHITypes.h)

- **IDevice** — creation and lifecycle: buffers, textures, shaders, pipelines, render targets, query pools, descriptor sets, bindless heap. Also command list creation/submit, immediate command list, shader path resolution, capabilities.
- **ICommandList** — recording: bind pipeline, vertex/index buffers, descriptor sets, draw/dispatch, render target, viewport, clear, copy, barriers.
- **IBuffer**, **ITexture** — opaque handles; backend-specific (OpenGL: VBO/ID, texture ID; Diligent: IBuffer/ITexture).
- **IShader**, **IPipeline** — compiled shader and graphics pipeline (vertex layout, topology, depth/stencil).
- **IRenderTarget** — framebuffer with color/depth attachments.
- **IPipelineLayout**, **IDescriptorSet** — for uniform/texture/sampler bindings (Vulkan-style; OpenGL emulates).
- **IBindlessHeap** — bindless texture/sampler handles when supported.
- **IQueryPool** — GPU timing (e.g. per-pass).

Resource states (e.g. `ShaderResource`, `RenderTarget`, `DepthWrite`) drive automatic barriers in the frame graph.

### 3.2 Backends

- **OpenGL** (`rhi/OpenGL/`): DeviceGL, CommandListGL (immediate + DeferredCommandListGL for record-then-submit), BufferGL, TextureGL, ShaderGL, PipelineGL, RenderTargetGL, DescriptorSetGL, QueryPoolGL, BindlessHeapGL. Shader path: `assets/shaders/opengl/{name}.vert/.frag`.
- **Diligent** (`rhi/diligent/`): Optional. DeviceDiligent, CommandListDiligent, etc. Created via `CreateDiligentDevice(nwh, width, height)`.

Device creation:

- `CreateOpenGLDevice()` — GL context must be current.
- `CreateGraphicsDevice(makeContextCurrent)` — calls `makeContextCurrent` then creates OpenGL device.
- `CreateDiligentDevice(nwh, w, h)` — when Diligent is available.

---

## 4. Scene Model & View

### 4.1 Types (RenderTypes.h)

- **Drawable** — mesh, material, materialInstance, transform, optional bones.
- **CameraView** — view/projection matrices, view position, near/far.
- **LightParams** — directional light (direction, color, intensity, ambient, exposure, castShadows).
- **PointLight**, **SpotLight** — std140 layout for Forward+ SSBOs.
- **RenderScene** — opaque/transparent drawables, camera, light, skybox, point/spot lights.

### 4.2 IRenderSceneView (IRenderSceneView.h)

Read-only interface used by the pipeline so it never copies full scene data:

- `GetCamera()`, `GetLight()`, `GetSkybox()`
- `ForEachOpaqueDrawable(cb)`, `ForEachTransparentDrawable(cb)`
- `ForEachPointLight(cb)`, `ForEachSpotLight(cb)`
- Optional: `GetSceneForCallback()` for code that needs a full `RenderScene*` (e.g. pre/post callback).

Implementations:

- **RenderSceneView** — wraps `const RenderScene&` (eyelib).
- **ApiRenderSceneView** (api/render) — wraps `const api::RenderScene&`; adapts API types to eyelib `Drawable`/`CameraView`/etc. so the pipeline sees the same interface.

---

## 5. Frame Graph

### 5.1 Responsibilities (framegraph/FrameGraph.h/cpp)

- **Resource declaration**: CreateTexture(desc) for transients; ImportTextureRHI(name, texture, w, h) for existing RHI textures (e.g. HDR, GBuffer, shadow).
- **Pass registration**: AddPass(name, execute, reads, writes, sideEffect). Reads/writes are `FGTextureHandle`s.
- **Compile(device)**:
  - Computes texture lifetimes and transient aliasing (same dimensions/format, non-overlapping lifetimes share RHI texture).
  - Builds DAG from read/write dependencies; topological sort → execution order. On cycle, falls back to insertion order.
  - Recomputes first/last pass per resource in execution order for correct barriers and aliasing.
- **Execute(cmd)**: Runs passes in computed order; inserts barriers so each resource is in the right state; runs execute callbacks with FGPassResources (getTexture / getTextureRHI, width, height).

No parameterless `Compile()` — device is required.

### 5.2 Recipe (FrameGraphRecipe.h/cpp)

- **FrameGraphPassDef** — name, reads[], writes[], sideEffect.
- **FrameGraphRecipe** — vector of pass defs.
- **AddPassesFromRecipe(graph, recipe, nameToHandle, getExecutor)** — resolves names to handles, adds passes to the graph; getExecutor(passName) returns the ExecuteFunc (typically closing over view, context, pipeline state).

Pipeline code builds a nameToHandle map (ImportTextureRHI / CreateTexture), defines a recipe, then calls AddPassesFromRecipe with a lambda that returns the right executor per pass.

---

## 6. Render Pipeline

### 6.1 Modes (RenderPipeline.h)

- **Forward** — Frame graph: GPU Cull → Shadow → Forward Opaque → Transparency → Skybox → Post. Single forward pass into HDR.
- **Deferred** — Frame graph: GPU Cull → Shadow → GBuffer → Copy depth (for occlusion) → Lighting → Transparency → Skybox → Post. Lighting reads GBuffer, writes HDR.
- **ForwardPlus** — Legacy path: tiled/clustered many lights; skybox after; no frame graph.

Render(view) dispatches by mode: Deferred → BuildAndExecuteFrameGraph; Forward → BuildAndExecuteForwardFrameGraph; ForwardPlus → BuildAndExecuteForwardPlusFrameGraph (single path for all).

### 6.2 Initialization & Lifecycle

- Initialize(unique_ptr&lt;IDevice&gt;) or Initialize(IDevice* sharedDevice), width, height.
- Resize(w, h) at frame boundary (GBuffer, HDR RT, post).
- Render(IRenderSceneView) or Render(RenderScene); optional entityIdsForOpaque for entity-ID pass (picking).
- BlitToScreen() to present.
- GetRHIDevice(), GetRHICommandList() for passes and asset loading that need the device.

### 6.3 Frame Graph Build (Deferred example)

1. Clear frame graph; set frame size.
2. Build nameToHandle: ImportTextureRHI for HDR, HDRDepth, GBuffer*, Shadow.
3. PrepareGpuCullFromView(view, cam) — build draw list, upload to GPU, configure GpuCullPass.
4. Define FrameGraphRecipe: GPU Cull, Shadow, GBuffer, Copy depth, Lighting, Transparency (and Skybox, Post via AddSkyboxPassToFrameGraph / PostProcessor).
5. getExecutor(name): each pass gets a lambda that captures pipeline, view, ctx, and runs the corresponding pass (e.g. GBuffer → `BuildGBufferOpaqueParams` + `GBufferPassSystem::RenderOpaqueGeometry`).
6. AddPassesFromRecipe; then Compile(device), Execute(ctx).

Forward frame graph is similar but with a single forward pass instead of GBuffer + Lighting.

---

## 7. Resources (CPU vs GPU Split)

### 7.1 Mesh

- **Mesh** (resources/Mesh.h): GPU-only. CreateFromDescriptor(device, desc) builds vertex/index buffers and submeshes. Bind(device), Draw(device, submeshIndex). Destroy(device). No CPU geometry stored.
- **MeshDescriptor** (MeshDescriptor.h): CPU-only; vertices, indices32/indices16, indexFormat, submeshes. Used by loaders and EditableMesh; Mesh::CreateFromDescriptor(device, desc) produces the GPU mesh.
- **MeshHandle**: Optional view (vertexBuffer, indexBuffer, indexType, totalIndexCount, submeshes) for draw path.
- **EditableMesh** (assets): CPU-authoritative; GetDescriptor(), BuildRenderMesh(out, device), CopyToRenderMesh(out, device).

### 7.2 Shader

- **Shader** (resources/Shader.h): GPU pipeline. Load(device, assetPath, layout, topology) or LoadFromFile(device, vertPath, fragPath, layout). BuildFromSource(device, ...). Bind(device). No SetDevice/GetDevice; device passed explicitly.
- **ShaderDescriptor**: CPU representation for serialization / creating another instance; LoadFromDescriptor(device, desc).

### 7.3 Textures

- **Texture2D / Texture3D / TextureCube / TextureArray**: Create(device, ...), LoadFromFile(device, path) or LoadFromDescriptor(device, desc). TextureCube has only device-taking LoadFromFiles(device, ...); no SetDevice/GetDevice.
- **TextureLoader** (resources/TextureLoader): LoadPNG(device, path), LoadHDR(device, path), CreateFromDecoded(device, img). Parameterless overloads remain for compatibility but prefer device-taking versions.
- **TextureDescriptor**: path + options; used for reload and asset storage.

### 7.4 Material / MaterialInstance / MaterialBuilder

- **Material**: Parameters and texture bindings (albedo, normal, metallic-roughness, etc.).
- **MaterialInstance**: Per-draw overrides; used at draw time with Material.
- **MaterialBuilder** (Refractor 7.5, Limitless-inspired): Fluent API to construct a Material (Albedo, Metallic, Roughness, textures, Blending, TwoSided, Refraction, ShaderIndex, snippets, etc.); `build()` returns `std::shared_ptr<Material>`.

---

## 8. Passes (High Level)

| Pass           | Role                                                                 |
|----------------|----------------------------------------------------------------------|
| **GpuCullPass**| Build draw list from view; optional GPU culling; indirect draw list.|
| **ShadowPass** | Render directional shadow map from light view.                       |
| **GBufferPassSystem**| Deferred geometry system: fill position, normal, albedo, PBR, velocity, depth. |
| **LightingPass**| Full-screen: sample GBuffer + shadow, write HDR.                    |
| **TransparencyPass**| Sorted transparent drawables over HDR.                             |
| **Skybox**     | Full-screen quad, depth LEQUAL, sample cubemap; drawn as frame-graph pass. |
| **PostProcessor**| Tone mapping, bloom, FXAA, SSAO, SSR, etc.; can be integrated as final frame-graph pass. |

---

## 9. API Layer (api/render) Summary

- **RenderDevice** — opaque device handle; obtained from pipeline (e.g. GetDevice()); passed to FrameGraph::Compile(device), texture loaders, asset registry.
- **RenderScene** — API scene (opaque/transparent storage, camera, light, skybox as api::TextureCube, etc.).
- **ApiRenderSceneView** — implements IRenderSceneView over api::RenderScene so RenderPipeline::Render(view) works without copying.
- **RenderPipeline** (api) — wraps eyelib RenderPipeline; InitializeWithSharedDevice(RenderDevice, w, h); Render(scene) builds ApiRenderSceneView and calls eyelib Render(view).
- **FrameGraph** (api) — Compile(RenderDevice device) only; no parameterless Compile().
- **TextureCube** (api) — LoadFromFiles(RenderDevice device, ...) only; forwards to eyelib TextureCube::LoadFromFiles(device, ...).

Applications (editor, player) get the device from the pipeline, pass it to asset loaders and skybox load so all GPU resource creation is explicit.

---

## 10. Architecture Rules (Explicit & Future-Proof)

### 10.1 Frame lifecycle (order is fixed)

`BeginFrame` → `UpdateUploads` → Compile frame graph → Execute frame graph → `EndFrame` → `Present`. All CPU→staging→copy uploads happen in `UpdateUploads()`; no uploads during pass execution.

### 10.2 Recording vs submission

- **Renderer** records commands only (into the command list provided by the device).
- **Device** owns submission: `EndFrame()` or `SubmitCommandList(list)` performs `vkQueueSubmit` / equivalent. The renderer must never submit directly.

### 10.3 FrameGraph is the only authority for transients

- **Transient textures** (GBuffer, HDR, shadow maps, post targets): only via `FrameGraph::CreateTexture()` or `FrameGraph::ImportTextureRHI()`. Pass execute callbacks must not create textures.
- **Barriers**: only the FrameGraph inserts transitions between passes (via Read/Write declarations and `TransitionPassResources`). Pass code must not call `BarrierTexture` / `TransitionTextureToState` for frame-graph resources; declare reads/writes so the graph inserts barriers.

### 10.4 Resource lifetime domains

| Domain | Examples | Rule |
|--------|----------|------|
| **Persistent** | Meshes, materials, textures, IBL, shadow cascade RTs | Created at load/init; live until unload. |
| **FrameGraph transient** | GBuffer, HDR, post buffers, shadow map targets in graph | Created/aliased by FrameGraph at Compile(); released on Clear(). |
| **Streaming** | Upload heap (staging), per-frame scratch | Per-frame or rolling; never mix with persistent or graph transients. |

Do not mix domains (e.g. do not create a persistent texture inside a pass execute callback).

### 10.5 Command lists are stateless across frames

`Begin()` clears all tracked state (bound pipeline, RT, textures, buffers, push constants). `End()` finalizes recording. No state survives across frames; each `Begin()` starts from a clean slate.

### 10.6 Descriptor binding (slot-based, backend owns layout)

Renderer binds by slot only: `cmd->BindTexture(tex, slot)`, `cmd->BindBuffer(buf, slot)`. The backend decides descriptor set index and layout (binding table or PushOnly). The renderer must not depend on descriptor set numbers or layout details.

---

## 11. Conventions

- **Pipeline warmup**: Pipelines are precompiled at init (e.g. `WarmupPipelines()`). `Draw()` must never create a pipeline.
- **Device**: Always pass `rhi::IDevice*` (or `api::RenderDevice`) into creation/load APIs; do not rely on a global or SetDevice for render resources.
- **Scene**: Pipeline takes `IRenderSceneView`; implement it over your scene type for zero-copy.
- **Frame graph**: Compile(device) required; execution order is DAG-derived when acyclic.
- **Mesh**: GPU-only in Mesh; CPU in MeshDescriptor / EditableMesh.
- **Shaders**: Load/Bind take device; backend resolves paths (e.g. assets/shaders/opengl/...).

---

**Refactor plan (clean, production-level):** See `documentation/refractor/Render_refractor.md` for the phased plan to split the pipeline into coordinators, introduce RenderFrameData/RenderWorldCache, submission layer, and editor/runtime split.

*Last updated to reflect explicit device passing, FrameGraph DAG/Compile(device), TextureCube device-only LoadFromFiles, and CPU vs GPU mesh/texture split.*
