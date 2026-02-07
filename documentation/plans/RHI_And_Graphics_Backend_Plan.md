# RHI and Graphics Backend Abstraction Plan

**Goal:** Make the render stack **backend-agnostic** by introducing a **low-level, explicit RHI (Render Hardware Interface)** and implementing it for OpenGL and Vulkan. The render module (pipeline, passes) will use the RHI (and a thin helper layer on top) so that no GL or Vulkan calls live outside the RHI layer. Target: **Unreal-level structure, control, and scalability**.

**Scope:** `eyelib/render/`, `eyelib/platform/` (context/device), `eyelib/fluids/` (FluidVolumeRenderer), `eyelib/debug/` (Profiler). All other modules (animation, assets, core, ecs, geometry, math, physics, scene, scripting, serialize, world, Tienet) remain unchanged.

**References:** `Render_Thread_Unified_Scene_Plan.md` (thread model, unified scene, frame allocator — already done); this plan builds on that and adds the abstraction layer.

---

## 1. Current State

### 1.1 Where OpenGL Lives Today

| Location | What uses GL |
|----------|----------------|
| **eyelib/render/** | Entire module: pipelines, passes (GBuffer, Shadow, Lighting, Transparency), resources (Mesh, Shader, Texture2D/3D/Array/Cube), FrameGraph, RenderTarget, FullscreenQuad, IBL, GBuffer, ShadowMap, TiledLightGrid, ClusteredLightGrid, PostProcessor, 2d (Text, Sprite, Primitives, Font), DebugDrawRenderer, InstancedRenderer, Renderer. |
| **eyelib/platform/window/** | `Window.cpp`: GL context creation, `gladLoadGLLoader`, make-current. |
| **eyelib/fluids/** | `FluidVolumeRenderer.cpp`: shaders, buffers, draw/dispatch. |
| **eyelib/debug/** | `Profiler.cpp`: `GL_TIME_ELAPSED` queries. |

No other eyelib modules contain OpenGL (or Vulkan) code.

### 1.2 What Stays Unchanged

- **api/render/** — Public API (e.g. `RenderPipeline::Render(scene)`). It will still forward to eyelib; only the eyelib implementation will call RHI instead of GL.
- **Game/render thread model** — Render thread owns all GPU work; game thread builds snapshot. Unchanged.
- **Unified RenderScene, frame allocator, immutable snapshot** — Unchanged; they are data shapes, not backend-specific.

---

## 2. Target Architecture: Three Layers

```
┌─────────────────────────────────────────────────────────────────┐
│  Render module (RenderPipeline, passes, resources, 2d, etc.)    │  ← High-level logic
│  Uses: RHI + RHIUtils only                                       │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  RHI Utils (optional thin layer)                                │  ← Small helpers
│  ClearAndSetRenderTarget, DrawFullscreenQuad, BarrierForRead,   │
│  UploadBuffer, etc. — all implemented with low-level RHI calls  │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  RHI (low-level, explicit)                                       │  ← Abstraction
│  IDevice, ICommandList, IBuffer, ITexture, IShader, IPipeline,   │
│  IRenderTarget, barriers, bind, draw, dispatch                  │
└─────────────────────────────────────────────────────────────────┘
                                    │
              ┌─────────────────────┴─────────────────────┐
              ▼                                             ▼
┌─────────────────────────┐                   ┌─────────────────────────┐
│  RHI::OpenGL            │                   │  RHI::Vulkan            │
│  (thin GL wrapper)      │                   │  (VkDevice, VkPipeline,  │
│                         │                   │   command buffers, etc.) │
└─────────────────────────┘                   └─────────────────────────┘
```

**Design rule:** RHI is **low-level and explicit**. The render module retains full control over when barriers run, what is bound, and when draws happen. RHI does not hide the GPU; it translates explicit intent to backend calls.

---

## 3. Folder Structure (Under eyelib/render/)

```
eyelib/render/
├── rhi/                              # Graphics backend abstraction
│   ├── RHI.h                         # Core interfaces + forward decls
│   ├── RHITypes.h                    # Enums, structs (usage, load/store, topology, etc.)
│   ├── RHI.cpp                       # Optional: shared RHI helpers (no backend)
│   ├── RHIUtils.h                    # Thin helpers built only from RHI calls
│   ├── RHIUtils.cpp                  #
│   ├── OpenGL/
│   │   ├── DeviceGL.h / .cpp
│   │   ├── CommandListGL.h / .cpp
│   │   ├── BufferGL.h / .cpp
│   │   ├── TextureGL.h / .cpp
│   │   ├── ShaderGL.h / .cpp
│   │   ├── PipelineGL.h / .cpp
│   │   ├── RenderTargetGL.h / .cpp
│   │   ├── QueryPoolGL.h / .cpp      # For Profiler (timestamp queries)
│   │   └── ...
│   └── Vulkan/
│       ├── DeviceVk.h / .cpp
│       ├── CommandListVk.h / .cpp
│       ├── BufferVk.h / .cpp
│       ├── TextureVk.h / .cpp
│       ├── ShaderVk.h / .cpp
│       ├── PipelineVk.h / .cpp
│       ├── RenderTargetVk.h / .cpp
│       ├── QueryPoolVk.h / .cpp
│       └── ...
├── passes/                           # Unchanged layout; implementation uses RHI
├── resources/                        # Mesh, Material, etc. hold RHI resource handles
├── framegraph/
├── RenderPipeline.cpp               # Uses RHI::IDevice + RHIUtils
├── ...                               # Rest of existing files
```

- **rhi/** contains the abstraction and all backends. No GL or Vulkan includes outside `rhi/OpenGL/` and `rhi/Vulkan/`.
- **RHIUtils** lives next to RHI so passes and pipeline can include one place for “common patterns”.

---

## 4. RHI Design (Low-Level, Explicit)

### 4.1 Core Abstractions

| Concept | Purpose |
|--------|---------|
| **IDevice** | Create buffers, textures, shaders, pipelines, render targets, query pools. Get the active command list. Present (swapchain). Backend is chosen at device creation (platform layer). |
| **ICommandList** | Record GPU work: set render target, clear, barriers, bind pipeline, bind resources (buffers/textures/samplers), draw (indexed/non-indexed), dispatch compute. No “magic”; each call maps to explicit backend behaviour. |
| **IBuffer** | GPU buffer (vertex, index, uniform, storage). Created with **usage flags** (e.g. Vertex, Index, Uniform, TransferDst, ShaderResource). Optional CPU visibility for upload. |
| **ITexture** | 2D / 2D array / cube / 3D. Created with **usage** (RenderTarget, ShaderResource, TransferDst, etc.) and **format**. |
| **IShader** | Compiled shader stage (vertex, fragment, compute). Backend compiles from source or bytecode. |
| **IPipeline** | Pipeline state: shader stages + vertex layout + blend/depth/raster state. Immutable (or cached by backend). |
| **IRenderTarget** | Framebuffer: colour + depth attachments. Resolve from one to another if needed. |
| **IQueryPool** | Timestamp or occlusion queries. Begin/End on command list; get results on device. |

### 4.2 Resource States / Barriers

- **Explicit transitions:** Before using a texture/buffer in a new way, the renderer issues a **barrier** (e.g. “texture from RenderTarget → ShaderResource”). RHI translates to:
  - **OpenGL:** minimal (layout changes implied by bind).
  - **Vulkan:** `VkImageMemoryBarrier` / `VkBufferMemoryBarrier` with correct `oldLayout`/`newLayout` and access masks.
- API shape (conceptual): `CommandList::Barrier(ResourceBarrier{ resource, fromState, toState })` or per-resource-type overloads.

### 4.3 Command List Semantics

- **Recording:** Passes and pipeline obtain a command list from the device and record into it (set RT, clear, barrier, bind PSO, bind descriptor set or equivalent, draw).
- **Submission:** Device submits the command list (OpenGL: execute immediately or queue; Vulkan: submit to queue). Render thread owns submission.
- **No hidden state:** Each “bind” and “draw” is explicit. Double-buffering of uniform buffers etc. is the render module’s responsibility; RHI just provides the buffer and bind API.

### 4.4 Shader and Pipeline

- **Shader:** Create from source (e.g. GLSL or SPIR-V for Vulkan). RHI may compile to backend format (GL shader object, VkShaderModule).
- **Pipeline:** Combination of shader stages + vertex input layout + blend/depth/raster. Created once and reused. Render module builds and caches pipelines; RHI just creates from descriptor.

### 4.5 What RHI Does *Not* Do

- No scene graph, culling, or batching — that stays in the render module.
- No “render this mesh” high-level call — only “bind pipeline, bind vertex/index buffers, draw”.
- No asset loading — only “create buffer/texture/shader from data”. Asset loaders (e.g. ModelLoader) produce data; render module creates RHI resources from that data.

---

## 5. RHI Utils (Thin Helper Layer)

A **small** set of helpers implemented **only** with RHI calls. No GL/Vulkan; no high-level logic.

| Helper (example names) | What it does |
|------------------------|--------------|
| **ClearAndSetRenderTarget(cmd, rt, clearColor, clearDepth)** | Barrier RT to write → set render target → clear colour/depth. |
| **DrawFullscreenQuad(cmd, pipeline, bindings)** | Bind pipeline + fullscreen quad VB (or no VB and vertex ID) + optional texture/sampler; one draw. |
| **BarrierTextureForShaderRead(cmd, texture)** | Single barrier: texture from current state to shader read. |
| **BarrierTextureForRenderTarget(cmd, texture)** | Texture to render target. |
| **UploadBuffer(device, buffer, data, size)** | Map or staging copy to get data into buffer (backend-appropriate). |
| **CopyTexture(cmd, src, dst)** | Copy between textures (e.g. resolve, or blit). |

- Implemented in **RHIUtils.cpp** (or a couple of files under `rhi/`). Used by **RenderPipeline** and **passes** so they don’t repeat the same RHI sequences everywhere.
- When a pass needs full control, it uses **ICommandList** directly and skips utils.

---

## 6. Render Module Changes (High Level)

- **RenderPipeline:** Holds `IRHIDevice*` (or shared_ptr). No `#include <glad/glad.h>`. Builds frame by getting command list, then calling passes. Each pass receives command list + RHI resources (render targets, textures, buffers, pipelines).
- **Passes (GBuffer, Shadow, Lighting, Transparency, PostProcessor, etc.):** Signature becomes “(ICommandList*, pass-specific RHI resources)”. Inside they use RHI (set RT, clear, barrier, bind PSO, bind textures/buffers, draw). Use **RHIUtils** for common patterns (e.g. fullscreen quad, clear RT).
- **Resources (Mesh, Texture2D, etc.):** Store RHI handle (`IBuffer*`, `ITexture*`, etc.) and create/update via `IDevice`. Upload and layout transitions go through RHI (or RHIUtils).
- **FrameGraph:** Can stay as a high-level pass graph; execution translates to RHI command list recording (create transient RHI resources per frame if needed, or reuse).

No change to **data flow** (unified RenderScene, frame allocator, immutable snapshot); only the **execution** of “how we draw” moves from GL to RHI.

---

## 7. Platform Layer (Context / Device Creation)

- **Current:** `Window` creates GL context and loads GL with glad.
- **Target:** Abstract “create graphics device” so the application (or platform) can choose backend:
  - **OpenGL:** Create GL context (GLFW + glad) and return an `IDevice` implemented by `DeviceGL`.
  - **Vulkan:** Create `VkInstance`, `VkSurface`, `VkDevice`, queues, swapchain; return `IDevice` implemented by `DeviceVk`.
- **Placement:** Platform can own a small “GraphicsContextFactory” or “CreateDevice(Backend::OpenGL | Vulkan, window)” that lives in `platform/` and includes only the chosen backend (or both and selects at runtime). RHI backends (DeviceGL, DeviceVk) are created there; render module only receives `IDevice*`.

---

## 8. Other Modules (Fluids, Debug)

- **eyelib/fluids/FluidVolumeRenderer:** Replace every GL call with RHI: create buffers/textures/shaders via `IDevice`, record draws/dispatches in `ICommandList`. Use RHIUtils where it fits. One file; localized change.
- **eyelib/debug/Profiler:** Replace `GL_TIME_ELAPSED` with RHI: `IQueryPool` for timestamps, `CommandList::BeginQuery` / `EndQuery`, `IDevice::GetQueryResult`. Profiler logic (scopes, aggregation) unchanged.

---

## 9. Phased Implementation

### Phase 0 — RHI Design and OpenGL Backend (Foundation)

**Goal:** Define RHI interfaces and implement them for OpenGL so that existing behaviour can be moved over incrementally.

**Tasks:**

1. Add **rhi/** under **eyelib/render/** with:
   - **RHITypes.h:** Enums (BufferUsage, TextureUsage, LoadAction, StoreAction, PrimitiveTopology, etc.), structs (VertexAttribute, BlendState, DepthStencilState, RasterState, PipelineDesc, etc.).
   - **RHI.h:** Interfaces (IDevice, ICommandList, IBuffer, ITexture, IShader, IPipeline, IRenderTarget, IQueryPool) and factory/create methods. No implementation; only declarations and minimal inline.
2. Implement **rhi/OpenGL/** for every interface (DeviceGL, CommandListGL, BufferGL, TextureGL, ShaderGL, PipelineGL, RenderTargetGL, QueryPoolGL). Each wraps existing GL concepts (VBO, texture, program, FBO, query). Implement barriers as no-ops or minimal GL sync where needed.
3. **Build:** RHI and OpenGL backend compile and link; no callers yet. Optional: small test that creates a device, buffer, and draws a triangle (proves the RHI path works).

**Deliverables:** `rhi/RHITypes.h`, `rhi/RHI.h`, `rhi/OpenGL/*` implemented. CMake updated so `EyelibRender` (or equivalent) can build with RHI + OpenGL.

**Exit criterion:** RHI + OpenGL backend builds; one optional end-to-end test (device → buffer → draw) passes.

---

### Phase 1 — Refactor Render Module to Use RHI (OpenGL Only)

**Goal:** All GL calls in the render module go through RHI. Render thread still uses only one backend (OpenGL).

**Tasks:**

1. **RenderPipeline:** Inject or obtain `IDevice*` (from platform or a render context). Replace direct GL in `RenderPipeline.cpp` with RHI: get command list, call passes with command list + RHI resources. Remove `#include <glad/glad.h>` from pipeline and passes.
2. **Passes:** Refactor each pass (GBuffer, Shadow, Lighting, Transparency, PostProcessor, etc.) to take `ICommandList*` and RHI resources. Inside: set render target via RHI, clear via RHI, barriers via RHI, bind pipeline and resources, draw via RHI. Use **RHIUtils** where a pattern repeats (e.g. clear RT, fullscreen quad).
3. **Resources:** Refactor Mesh, Texture2D/3D/Array/Cube, Shader (if still used at this level), RenderTarget, etc., to hold RHI handles and create/update through IDevice. Ensure upload paths (e.g. mesh vertex/index data, texture pixels) go through RHI buffers/textures.
4. **FrameGraph:** If it currently issues GL, change it to issue RHI command list calls and use RHI resources for attachments.
5. **2d, IBL, ShadowMap, LightGrids, FullscreenQuad, DebugDrawRenderer, InstancedRenderer:** Same rule — no GL; only RHI (and RHIUtils). Remove all `#include <glad/glad.h>` from **eyelib/render/** except inside **rhi/OpenGL/**.

**Deliverables:** Render pipeline and all passes/resources use RHI only. OpenGL backend is the only backend; behaviour matches current (visually and functionally).

**Exit criterion:** Player runs with OpenGL backend; no regressions; no GL includes outside `rhi/OpenGL/`.

---

### Phase 2 — RHI Utils and Cleanup

**Goal:** Introduce RHIUtils and remove duplicated RHI call sequences from passes.

**Tasks:**

1. Add **RHIUtils.h/cpp** in **rhi/** with helpers (ClearAndSetRenderTarget, DrawFullscreenQuad, BarrierTextureForShaderRead, BarrierTextureForRenderTarget, UploadBuffer, etc.).
2. Refactor passes to use RHIUtils where it shortens and clarifies code. Keep direct ICommandList use where control is needed.
3. Document which RHI concepts each pass uses (for future Vulkan tuning).

**Deliverables:** RHIUtils in place; passes cleaned up; doc or comments on pass–RHI usage.

**Exit criterion:** Passes use RHIUtils where appropriate; no new GL; behaviour unchanged.

---

### Phase 3 — Platform Device Abstraction

**Goal:** Platform exposes “create graphics device” in a backend-agnostic way; render module receives IDevice from platform.

**Tasks:**

1. In **platform**, add a way to create an RHI device (e.g. `CreateGraphicsDevice(Window*, Backend::OpenGL)`). For OpenGL, this creates the GL context (as today) and returns `new DeviceGL(...)` or similar. Window may still own the surface; device owns the context/device.
2. Render pipeline (or engine) gets IDevice from platform at init; uses it for all render work. No GL context creation in render module.
3. Swapchain/present: either IDevice::Present() or platform presents using device’s backbuffer. Decide and implement so that current “blit to window” behaviour is preserved.

**Deliverables:** Platform creates IDevice; render uses that device only. Clear separation: platform = context/device creation; render = recording and submission.

**Exit criterion:** Run with OpenGL; no GL context creation in render; present works.

---

### Phase 4 — Vulkan Backend

**Goal:** Implement RHI for Vulkan; application can choose Vulkan at startup and run the same render path.

**Tasks:**

1. Implement **rhi/Vulkan/** for every interface: DeviceVk, CommandListVk (command buffers, possibly secondary), BufferVk, TextureVk (with VkImageLayout and barriers), ShaderVk (SPIR-V), PipelineVk, RenderTargetVk (VkFramebuffer), QueryPoolVk (timestamp).
2. Shader path: either ship SPIR-V for Vulkan and GLSL for GL, or compile from a single source (e.g. glslang for Vulkan, keep GLSL for GL). Define strategy and implement.
3. Barriers: implement full barrier API in CommandListVk (image and buffer barriers, correct stages and access masks).
4. Platform: add Vulkan device creation path (instance, surface, physical device, logical device, queues, swapchain). Choose surface from window (e.g. GLFW for Vulkan surface).
5. Integration: when backend is Vulkan, platform creates DeviceVk; pipeline and passes unchanged, they just use the Vulkan backend. Fix any backend-specific assumptions (e.g. texture format, clear behaviour).

**Deliverables:** Vulkan backend implemented; optional runtime or compile-time switch to use Vulkan; same scene renders correctly (with possible minor differences).

**Exit criterion:** Run with Vulkan backend; no crashes; correct image; performance acceptable for a first version.

---

### Phase 5 — Fluids and Debug on RHI

**Goal:** No GL left in fluids or debug.

**Tasks:**

1. **FluidVolumeRenderer:** Already refactored if it uses render module resources; if it has its own GL, replace with RHI (device, command list, buffers, textures, pipelines, draw/dispatch).
2. **Profiler:** Replace GL queries with IQueryPool. BeginQuery/EndQuery on command list; get results from device. Profiler logic unchanged.

**Deliverables:** Zero GL in fluids and debug; both use RHI only.

**Exit criterion:** Profiler works with both OpenGL and Vulkan; fluids render correctly with both backends.

---

## 10. Migration and Risk

- **Incremental:** Phase 1 can be done pass-by-pass (e.g. move one pass to RHI, test, then next). Reduces risk of big-bang rewrite.
- **Testing:** Keep existing tests and player; after each phase, run and compare (visually or with a baseline). Add RHI-only unit tests if useful (e.g. create device, create buffer, create pipeline, draw).
- **Rollback:** Until Phase 1 is complete, keep the old GL path behind a compile flag or branch if needed; drop it once RHI path is default and stable.
- **Performance:** OpenGL backend should be thin; expect similar performance. Vulkan may need tuning (barrier batching, descriptor set reuse, etc.) in a later pass.

---

## 11. Success Criteria (Overall)

- **Backend-agnostic:** No direct GL or Vulkan calls in render, fluids, or debug; all go through RHI (and RHIUtils).
- **Explicit control:** Render module decides barriers, bind order, and draw order; RHI only translates to backend.
- **Dual backend:** Same code path runs on OpenGL and Vulkan; only device creation and RHI implementation differ.
- **Structure:** Clear separation: rhi/ (abstraction + OpenGL + Vulkan), RHIUtils, then pipeline and passes. Unreal-level organisation and control.

---

## 12. Document History

- **Created:** RHI and graphics backend abstraction plan; three-layer design (RHI, RHIUtils, render module); folder structure; phased implementation (0–5); platform, fluids, debug scope.

---

## 13. How far done (current status)

**Last updated:** After device injection, pipeline/passes using RHI only (no glad), and RenderBindings removed in favour of RHI texture bind/unbind and ShadowMap RHI wrapper.

| Phase | Name | Status | Notes |
|-------|------|--------|--------|
| **0** | RHI Design and OpenGL Backend | **Done** | `rhi/RHI.h`, `RHITypes.h`, `rhi/OpenGL/*` (DeviceGL, CommandListGL, BufferGL, TextureGL, ShaderGL, PipelineGL, RenderTargetGL, QueryPoolGL). Pipeline and passes use RHI; build uses RHI + OpenGL only. |
| **1** | Refactor Render Module to Use RHI | **In progress** | Pipeline: `IDevice` injected, no glad; uses command list and RHIUtils. **Passes GL-free:** ShadowPass, GBufferPass, LightingPass, TransparencyPass no longer include glad; they take `rhi::ICommandList* cmd` and use RHI state. RHI extended with `SetCullFrontFace(bool)`, `SetBlend(bool)`. **ShadowMap:** RHI-only. **FrameGraph GL-free:** transient textures created via `IDevice::CreateTexture`, timing via `IQueryPool` and `ICommandList::BeginQuery`/`EndQuery`; `SetDevice`/`Execute(cmd)`; no glad in `framegraph/`. **RenderTarget GL-free:** Create(device, ...) only; Bind/Unbind/Resolve no-op; Blit via IDevice::Blit; GetFBO/GetColorTexture via RHI; Texture2D::WrapRHI for RT texture wrappers; IDevice::GetMaxSamples. **Still using GL:** GBuffer (FBO bind), resources (Mesh, Shader, Texture2D/3D/Array/Cube), PostProcessor, FullscreenQuad, ClusteredLightGrid, IBL, 2d, DebugDrawRenderer, InstancedRenderer, etc. |
| **2** | RHI Utils and Cleanup | **Done** | RHIUtils: `ClearAndSetRenderTarget`, `ClearAndSetRenderTargetWithViewport` (set RT + viewport + clear), `DrawFullscreenQuad`, `BarrierTextureForShaderRead`/`ForRenderTarget`. RenderPipeline and ShadowMap use `ClearAndSetRenderTargetWithViewport` for all full-RT clear setup; repeated SetRenderTarget/SetViewport/ClearRenderTarget blocks removed. |
| **3** | Platform Device Abstraction | **Done** | **Device factory:** `rhi::CreateOpenGLDevice()`, `rhi::CreateGraphicsDevice(makeContextCurrent)`, and **platform:** `platform::CreateGraphicsDevice(Window*)` in `eyelib/platform/window/GraphicsDevice.h` (declaration); implementation in render's `CreateDevice.cpp` (makes context current, then creates device). Api has `RenderPipeline::Initialize(Window& window, width, height)` that uses `CreateGraphicsDevice([&window]{ window.MakeContextCurrent(); })`. App (PlayerApp) calls `renderBridge.Initialize(window, width, height)`. No circular dependency: platform header includes RHI for return type; implementation lives in render. |
| **4** | Vulkan Backend | **Not started** | — |
| **5** | Fluids and Debug on RHI | **Not started** | FluidVolumeRenderer and Profiler still use GL directly. |

**Summary**

- **Done:** RHI + OpenGL backend in place; pipeline and main passes go through RHI only (no glad in `RenderPipeline.cpp` or pass code). Shadow map binding is fully RHI (CreateTextureWrapper, `cmd->BindTexture` / `UnbindTexture`). RenderBindings removed. Device is injected; render module does not create the GL context.
- **In progress:** Full migration of all resources (Mesh, Texture2D, Shader, etc.) to RHI handles.
- **Next:** Vulkan (Phase 4), Fluids/Debug on RHI (Phase 5), or Phase 6 from Render_Thread plan (asset/shader loading).
