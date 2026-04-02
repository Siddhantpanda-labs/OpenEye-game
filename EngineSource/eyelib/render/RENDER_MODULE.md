# Render Module — Structure & Architecture

This document describes the structure and architecture of the render module: **eyelib/render** (core) and **api/render** (public API layer).

---

## 1. Overview

The render module provides:

- **RHI (Render Hardware Interface)** — backend-agnostic abstraction over **OpenGL**, **Vulkan** (when enabled), and optional **Diligent Engine**.
- **Scene model** — drawables, camera, lights, skybox; consumed via a read-only view interface.
- **Frame graph** — declarative pass ordering with DAG-based execution and transient texture aliasing.
- **Pipeline modes** — Deferred (GBuffer + lighting), Forward (frame-graph), and Forward+ (legacy tiled).
- **Resources** — GPU-only mesh, shaders, materials, textures (2D, 3D, cube, array); CPU data lives in descriptors / `EditableMesh`.

Design principles:

- **Explicit device passing** — RHI and resource creation/load APIs take `rhi::IDevice*` (or `api::RenderDevice` at API layer); no global or injected device for render resources.
- **Zero-copy scene view** — Pipeline renders from `IRenderSceneView`; API and ECS both implement it over their scene data without copying.
- **CPU vs GPU split** — Mesh is GPU-only; CPU geometry lives in `MeshDescriptor` / `EditableMesh`. Textures load/create with an explicit device.
- **Single active recording buffer (Vulkan + editor)** — Per-frame GPU work that must land on the buffer that will be submitted uses `IDevice::GetActiveRecordingCommandList()` (transient slice when set, else immediate). See §3.4.
- **Device-owned submission** — Recording happens on command lists; **queue submit** is performed by the device (`EndFrame`, `SubmitCommandList`), not by ad-hoc callers.

---

## 2. Directory Layout

### 2.1 eyelib/render (core)

```
eyelib/render/
├── rhi/                    # RHI interface and backends
│   ├── RHI.h               # IDevice, ICommandList, GetActiveRecordingCommandList, …
│   ├── RHITypes.h          # BufferDesc, TextureDesc, VertexLayout, enums (ResourceState, …)
│   ├── CreateDevice.h/cpp  # CreateOpenGLDevice(), CreateGraphicsDevice(), CreateDiligentDevice()
│   ├── RHIUtils.cpp
│   ├── OpenGL/             # DeviceGL, CommandListGL, DeferredCommandListGL, …
│   ├── Vulkan/             # DeviceVK, CommandListVK, TextureVK, barriers, descriptors, …
│   └── diligent/           # Optional when OPENEYE_DILIGENT_AVAILABLE
├── framegraph/
│   ├── FrameGraph.h/cpp
│   └── FrameGraphRecipe.h/cpp
├── renderer/               # Pipeline orchestration + graph wiring
│   ├── RenderPipeline.h/cpp
│   ├── RenderPipelineGraph.cpp
│   └── graph/              # Recipes, FrameGraphPassNames, …
├── passes/
│   ├── GBufferPassSystem, GpuCullPassSystem, LightCullPassSystem, SkyboxPassSystem, …
│   ├── Forward*Geometry*, TransparencyPassSystem, DeferredLightingPassSystem, …
│   └── detail/             # GpuCullPass, LightCullPass, …
├── resources/              # Mesh, Shader, textures, TextureLoader, …
├── post/                   # PostProcessor
├── shadows/, ibl/, materials/, 2d/, …
├── IRenderSceneView.h
├── RenderTypes.h
├── RenderContext.h         # { device, cmd } — cmd = active recording list
├── PassPushConstants.h     # Shared GPU push layouts (domain-scoped over time)
├── DebugDrawRenderer.cpp
└── RENDER_MODULE.md        # This file
```

### 2.2 api/render (public API)

- Thin wrappers around eyelib types: **RenderPipeline**, **RenderScene**, **Renderer**, **Mesh**, **Material**, **Shader**, **Texture**, **TextureCube**, **SkyboxCubemapLoad**, **GBuffer**, **PostProcessor**, **IBL**, etc.
- **RenderSceneAdapter**: `api::RenderScene` → `ApiRenderSceneView` implementing `eyelib::render::IRenderSceneView` (zero-copy).
- **RenderDevice**: opaque handle (`void*`) for passing the RHI device from pipeline to asset loaders and other systems.
- **FrameGraph**: API `Compile(RenderDevice device)` forwards to eyelib `Compile(rhi::IDevice*)`; no parameterless overload.

---

## 3. RHI (Render Hardware Interface)

### 3.1 Interface (rhi/RHI.h, RHITypes.h)

- **IDevice** — creation and lifecycle: buffers, textures, shaders, pipelines, render targets, query pools, descriptor sets, bindless heap. Command lists: `CreateCommandList`, `DestroyCommandList`, `GetImmediateCommandList`, optional transient recording, `GetActiveRecordingCommandList`, `EnqueueFrameSliceCommandList`, `SubmitCommandList`. Shader path resolution, `GetCapabilities()`, `GetRenderBackend()`.
- **ICommandList** — recording: bind pipeline, vertex/index buffers, descriptor sets, draw/dispatch, render target, viewport, clear, copy, barriers.
- **IBuffer**, **ITexture** — opaque handles; backend-specific (GL IDs, Vulkan `VkBuffer`/`VkImage`, etc.).
- **IShader**, **IPipeline** — compiled shader and graphics/compute pipelines.
- **IRenderTarget** — framebuffer / swapchain-relative targets.
- **IPipelineLayout**, **IDescriptorSet** — bindings (Vulkan-native; OpenGL emulates).
- **IBindlessHeap**, **IQueryPool** — optional capabilities.

Resource states drive barriers (frame graph + `BarrierTracker` on Vulkan).

### 3.2 Backends

- **OpenGL** (`rhi/OpenGL/`): immediate command list + deferred record-and-replay for `CreateCommandList`/`SubmitCommandList`. Shader assets: `assets/shaders/opengl/{name}.vert/.frag`.
- **Vulkan** (`rhi/Vulkan/`): `DeviceVK`, `CommandListVK`, per-frame command pool and fence, swapchain acquire/present, `UploadHeapVK`, `BarrierTrackerVK`, descriptor allocator/cache, graphics pipeline cache. Shader assets: `assets/shaders/vulkan/` (SPIR-V path). Built when `OPENEYE_USE_VULKAN` and the SDK are available.
- **Diligent** (`rhi/diligent/`): Optional. `CreateDiligentDevice(nwh, width, height)`.

Device creation (typical):

- `CreateOpenGLDevice()` — GL context must be current.
- `CreateGraphicsDevice(makeContextCurrent)` — often selects OpenGL after making context current.
- Vulkan device creation via the project’s Vulkan bootstrap (see `rhi/Vulkan/core/CreateDeviceVulkan.cpp` and CMake options).

### 3.3 Vulkan frame submission model

- **`BeginFrame`**: advances frame ring, waits fence, resets command pool, resets per-frame upload/descriptor/barrier state. The **main** immediate command buffer is prepared but **recording starts lazily** (after optional editor slices) so embedded views do not corrupt swapchain composition order.
- **`EndFrame`**: collects **all enqueued slice command buffers** for the current frame slot, then **ends and appends the main immediate buffer**, and issues **one** `vkQueueSubmit` with slices **first**, main buffer **last** (see `DeviceVK::EndFrame`).
- **`SubmitCommandList`**: separate submit path for ad-hoc lists (e.g. async upload with timeline semaphore).

### 3.4 Active recording command list & editor slices

**Problem:** In the editor, **Scene View** and **Game View** render to **offscreen** pipelines sharing the same `IDevice` as the main frame. Recording their draws on the **same** VkCommandBuffer as swapchain composition would interleave unrelated passes and break ordering/state.

**Mechanism:**

1. **`IDevice::GetTransientRecordingCommandList()`** / **`SetTransientRecordingCommandList(cmd)`** — Vulkan: while non-null, this buffer is the “current slice” target.
2. **`IDevice::GetActiveRecordingCommandList()`** — returns transient if set, **else** `GetImmediateCommandList()`. All code that records per-frame GPU work that must participate in the correct submit should use this (not `GetImmediateCommandList()` alone).
3. **`EnqueueFrameSliceCommandList(cmd)`** — takes ownership of a finished list; device stores it in the **current frame ring** for inclusion in `EndFrame`’s submit batch.

**Editor integration** (application layer): `OpenEyeEditor` uses **`VulkanRenderSliceScope`** (`app/editor/EditorVulkanRenderSlice.cpp`): on enter — `CreateCommandList`, `Begin`, `SetTransientRecordingCommandList`; on exit — clear transient, `End`, `EnqueueFrameSliceCommandList`. Used around **SceneViewPanel** and **GameViewPanel** render paths when the backend is Vulkan. OpenGL: scope no-ops (backend check in ctor).

**Eyelib discipline:** `RenderContext::FromDevice()` sets **`cmd = GetActiveRecordingCommandList()`**. Systems updated to follow this include (non-exhaustive): **`Shader::Bind`** and uniform setters, **`Mesh`/`MeshHandle` bind/draw**, **`PostProcessor`**, **`DebugDrawRenderer`**, **`TextRenderer`**, **`IBL`** bake/bind paths, and pipeline code that builds `RenderContext` for passes.

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
- Optional: `GetSceneForCallback()` for code that needs a full `RenderScene*`.

Implementations:

- **RenderSceneView** — wraps `const RenderScene&` (eyelib).
- **ApiRenderSceneView** (api/render) — wraps `const api::RenderScene&`.

---

## 5. Frame Graph

### 5.1 Responsibilities (framegraph/FrameGraph.h/cpp)

- **Resource declaration**: `CreateTexture(desc)` for transients; `ImportTextureRHI(name, texture, w, h)` for existing RHI textures (e.g. HDR, GBuffer, shadow).
- **Pass registration**: `AddPass(name, execute, reads, writes, sideEffect)`.
- **Compile(device)**: lifetimes, transient aliasing, DAG topological order, barrier/alias metadata.
- **Execute(cmd)**: runs passes in order; transitions resources; callbacks receive `FGPassResources`.

No parameterless `Compile()` — device is required.

### 5.2 Recipe (FrameGraphRecipe.h/cpp)

- **AddPassesFromRecipe** — resolves pass names to handles and executor lambdas.
- Shared pass-name constants: **`renderer/graph/FrameGraphPassNames.h`** (reduces string drift between recipes and `RenderPipelineGraph`).

---

## 6. Render Pipeline

### 6.1 Modes (RenderPipeline.h)

- **Forward** — Frame graph: GPU Cull → Shadow → Forward Opaque → Transparency → Skybox → Post.
- **Deferred** — GPU Cull → Shadow → GBuffer → Copy depth → Lighting → Transparency → Skybox → Post.
- **ForwardPlus** — Legacy tiled/clustered path; no frame graph.

Graph execution and executor wiring are largely in **`RenderPipeline.cpp`** + **`RenderPipelineGraph.cpp`** (orchestration vs. graph recipes/executors).

### 6.2 Initialization & Lifecycle

- `Initialize` / shared device, `Resize`, `Render(IRenderSceneView)`, optional entity-ID pass for picking.
- **`GetRHIDevice()`**, **`GetRHICommandList()`** — the latter returns **`GetActiveRecordingCommandList()`** on the device (same buffer as `RenderContext::FromDevice` when building from device only).

### 6.3 Frame graph build (Deferred example)

1. Clear frame graph; set frame size.
2. Build `nameToHandle`: import HDR, depth, GBuffer, shadow, etc.
3. `PrepareGpuCullFromView` / GPU cull configuration.
4. Recipe: cull, shadow, GBuffer, copy depth, lighting, transparency, skybox, post.
5. `AddPassesFromRecipe` → `Compile(device)` → `Execute(ctx)` with `RenderContext` carrying **active** `cmd`.

---

## 7. Resources (CPU vs GPU Split)

### 7.1 Mesh

- **Mesh**: GPU-only; `CreateFromDescriptor(device, desc)`, `Bind`/`Draw` use **`GetActiveRecordingCommandList()`** on the device for Vulkan slice correctness.
- **MeshDescriptor** / **EditableMesh**: CPU-side geometry sources.

### 7.2 Shader

- **Shader**: `Load`/`Bind`/uniform setters use the **active** recording command list so pipeline binds record on the slice when the editor has set a transient list.

### 7.3 Textures

- **Texture2D / 3D / Cube / Array**: explicit `device` on create/load.
- **TextureLoader**: HDR paths use controlled `stbi_set_flip_vertically_on_load` scoping; cubemap loads pin flip off in **TextureCube** so global STB state does not leak between loaders.

### 7.4 Material / MaterialInstance / MaterialBuilder

- **Material** / **MaterialInstance**: parameters and texture bindings at draw time.
- **MaterialBuilder**: fluent construction of materials (see refactor docs).

---

## 8. Passes (High Level)

| Area | Role |
|------|------|
| **GpuCullPassSystem** | Draw list + optional GPU culling / indirect draws. |
| **ShadowPass** | Directional cascades. |
| **GBufferPassSystem** | Deferred geometry → GBuffer. |
| **DeferredLightingPassSystem** / **LightingPass** | Full-screen lighting from GBuffer + shadow. |
| **TransparencyPassSystem** | Sorted transparency. |
| **SkyboxPassSystem** | Cubemap or procedural sky (fullscreen, depth test). |
| **PostProcessor** | Tone map, bloom, FXAA, SSAO, SSR, etc.; binds correct descriptor sets for Vulkan post layout. |

---

## 9. API Layer (api/render) Summary

- **RenderDevice** — passed into `FrameGraph::Compile`, loaders, registry.
- **RenderPipeline** — `InitializeWithSharedDevice`; `Render(scene)` via `ApiRenderSceneView`.
- **SkyboxCubemapLoad** — shared cubemap resolve (cross layout vs. six faces) for editor and runtime bridges.

---

## 10. Architecture Rules (Explicit & Future-Proof)

### 10.1 Frame lifecycle (order is fixed)

`BeginFrame` → `UpdateUploads` → compile frame graph → execute frame graph → `EndFrame` → present. Staging uploads are concentrated in **`UpdateUploads`** where possible; avoid ad-hoc uploads mid-pass when a unified upload phase exists.

### 10.2 Recording vs submission

- **Passes** record into the **`ICommandList`** provided by context (**active** list).
- **Device** submits: **`EndFrame`** batches slice + main buffers (Vulkan); **`SubmitCommandList`** for standalone submits.

### 10.3 FrameGraph is the authority for graph transients

- Transient textures and inter-pass barriers come from graph **reads/writes** and compile-time lifetime analysis.

### 10.4 Resource lifetime domains

| Domain | Examples | Rule |
|--------|----------|------|
| **Persistent** | Meshes, materials, IBL, shadow RTs owned by pipeline | Until unload. |
| **FrameGraph transient** | GBuffer, HDR, post intermediates | Created at `Compile`; cleared with graph. |
| **Per-frame scratch** | Upload heap regions, transient descriptor sets | Ring-buffered per frame index. |

### 10.5 Command list state across frames

Each frame, pools reset (Vulkan) or lists are begun fresh; **do not** assume bindings survive across `BeginFrame` boundaries.

### 10.6 Descriptor binding

High-level code uses slot APIs (`BindTexture`, `BindBuffer`, …); backend maps to sets/bindings (Vulkan **Post** layout uses multiple sets — see post/bind passes).

### 10.7 New recording code checklist

1. Use **`GetActiveRecordingCommandList()`** (or **`RenderContext::FromDevice`**) for anything that must appear in the same submit batch as the rest of the frame **when editor slices are active**.
2. Do not **`vkQueueSubmit`** (or equivalent) from pass code.
3. For editor-only pre-composition work on Vulkan, prefer **`VulkanRenderSliceScope`** (or the same transient + enqueue pattern) instead of the main immediate buffer.

---

## 11. Conventions

- **Warmup**: Pipelines created at init where possible; avoid first-draw compile hitch.
- **Device**: always explicit `rhi::IDevice*` / `api::RenderDevice`.
- **Shaders**: backend folder under `assets/shaders/{opengl|vulkan|diligent}/…`.
- **Refactor roadmap**: `documentation/refractor/Render_refractor.md` and `Render_Report.md`.

---

*Last updated: Vulkan backend, per-frame slice submission, `GetActiveRecordingCommandList` / `RenderContext`, editor slice integration, graph split (`RenderPipelineGraph`), and pass-system naming.*
