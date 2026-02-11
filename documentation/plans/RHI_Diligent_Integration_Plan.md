# RHI Diligent Engine Backend Integration Plan

**Goal:** Add **Diligent Engine** as an optional RHI backend under the existing RHI layer. The render module (pipeline, passes, RHIUtils) stays unchanged; only the code under the RHI hood gains a Diligent implementation path. Integration is **minimal and flexible**: we align with Diligent’s design (explicit pipelines, descriptor sets, barriers) and implement the existing RHI interface in a Diligent-friendly way.

**Motivation:** Diligent provides a single integration that can target **D3D11, D3D12, Vulkan, OpenGL, Metal, WebGPU**. It is low-level and explicit (similar to our RHI design), uses **HLSL as universal shading language** (with GLSL/SPIR-V support), and is **Apache 2.0** licensed. Adding it alongside BGFX/OpenGL gives a path to modern APIs (D3D12, Vulkan) and cross-platform without maintaining multiple custom backends.

**Scope:** `eyelib/render/rhi/` (add `diligent/` backend; optional build), `eyelib/platform/` (device creation: add Diligent branch). **Unchanged:** `api/render/`, render module (RenderPipeline, passes, RHIUtils), game/render thread model, unified RenderScene.

**References:** `RHI_And_Graphics_Backend_Plan.md` (RHI design); `RHI_BGFX_Integration_Plan.md` (pattern for adding a backend).

---

## 1. Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Render module (RenderPipeline, passes, resources, etc.)         │  ← UNCHANGED
│  Uses: RHI + RHIUtils only                                       │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  RHI (same interfaces: IDevice, ICommandList, IBuffer, …)        │  ← UNCHANGED
└─────────────────────────────────────────────────────────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  RHI::OpenGL    │    │  RHI::BGFX      │    │  RHI::Diligent   │  ← NEW
│  (existing)     │    │  (existing)     │    │  DeviceDiligent, │
└─────────────────┘    └─────────────────┘    │  CommandList, …  │
                                               └─────────────────┘
```

- **Render module:** No code changes; it uses `ICommandList*`, RHI resources, and RHIUtils.
- **RHI interfaces:** No redesign; we implement them once with Diligent (DeviceDiligent, CommandListDiligent, BufferDiligent, etc.).

---

## 2. RHI → Diligent Mapping (Pragmatic)

| RHI concept       | Diligent mapping |
|-------------------|------------------|
| **IDevice**       | `IRenderDevice` + `ISwapChain`; create/destroy map to Diligent factory methods. Present → swap chain present. |
| **ICommandList**  | `IDeviceContext` (immediate or deferred). Begin/End → transition/scope; Set*/Bind*/Draw* map to Diligent context API. |
| **IBuffer**       | `IBuffer` (vertex, index, uniform, etc.). BufferDesc → Diligent buffer desc. |
| **ITexture**      | `ITexture` (2D, 2D array, cube, 3D). TextureDesc → Diligent texture desc; format from DiligentEnums. |
| **IShader**       | `IShader` from compiled bytecode/source. HLSL or GLSL path per backend. |
| **IPipeline**     | `IPipelineState` (graphics pipeline: shaders + layout + blend/depth/raster). |
| **IRenderTarget** | `IFramebuffer` (or swap chain RTV). MRT = framebuffer with multiple attachments. |
| **IQueryPool**    | Diligent query interface (timestamp/occlusion) if available; else stub. |
| **Barriers**      | Diligent resource transition API (`TransitionResourceStates`). |
| **Blit**          | Fullscreen pass or Diligent copy/blit where supported. |

We do **not** extend the RHI API for Diligent-specific features. We implement the existing RHI interface in `rhi/diligent/` only.

---

## 3. Folder Structure (Under eyelib/render/rhi/)

```
eyelib/render/rhi/
├── RHI.h, RHITypes.h, RHIUtils.h/cpp     # UNCHANGED
├── CreateDevice.h / .cpp                  # Extend: Diligent branch; CreateDiligentDevice when built
├── diligent/                              # NEW
│   ├── DeviceDiligent.h / .cpp
│   ├── CommandListDiligent.h / .cpp
│   ├── BufferDiligent.h / .cpp
│   ├── TextureDiligent.h / .cpp
│   ├── ShaderDiligent.h / .cpp
│   ├── PipelineDiligent.h / .cpp
│   ├── RenderTargetDiligent.h / .cpp
│   ├── QueryPoolDiligent.h / .cpp
│   ├── DiligentEnums.h / .cpp             # RHITypes ↔ Diligent format/state enums
│   └── CreateDeviceDiligent.h / .cpp
├── bgfx/                                   # UNCHANGED
└── OpenGL/                                 # UNCHANGED
```

Only `rhi/diligent/` (and platform device creation) include Diligent headers when the real implementation is enabled.

---

## 4. Phased Implementation

### Phase 0 — Stub backend (no Diligent dependency)

- Add `RenderBackend::Diligent` and `CreateDiligentDevice(void* nwh, uint32_t width, uint32_t height)`.
- Create `rhi/diligent/` with stub implementations of all RHI interfaces. DeviceDiligent creates stub buffers/textures/pipelines; CommandListDiligent no-ops; Present/Blit no-op. No Diligent Engine dependency; backend compiles and links with the rest of the project.
- Platform `CreateGraphicsDevice(..., RenderBackend::Diligent)` returns the stub device when Diligent backend is selected.
- **Exit criterion:** Build with Diligent backend option; app can select Diligent and get a non-crashing stub device.

### Phase 1 — Add Diligent Engine dependency and build

- Add CMake option `OPENEYE_USE_DILIGENT` (OFF by default). Add `FetchDiligent.cmake` (or document manual clone) to bring in DiligentEngine (and submodules: DiligentCore, etc.) under `thirdparty/DiligentEngine`.
- Build DiligentCore + one backend (e.g. OpenGL or D3D11) as static libs; link into EyelibRender when `OPENEYE_USE_DILIGENT` is ON.
- **Exit criterion:** With `OPENEYE_USE_DILIGENT=ON` and thirdparty present, Diligent libs build and link.

### Phase 2 — Real implementation

- Replace stub implementations in `rhi/diligent/` with Diligent API calls (IRenderDevice, IDeviceContext, IBuffer, ITexture, IPipelineState, etc.).
- Add DiligentEnums for format, resource state, and pipeline state mapping.
- Shader path: HLSL (or GLSL) compiled for the active Diligent backend; load via Diligent shader API.
- **Exit criterion:** One full frame renders through RHI with Diligent backend (e.g. clear + fullscreen quad or triangle).

### Phase 3 — Feature parity and cleanup

- Implement all IDevice/ICommandList methods (blit, texture wrapper, query pool, MSAA, etc.).
- Ensure all current render paths (forward, deferred, shadows, post-process) work with Diligent backend.
- **Exit criterion:** No regressions vs OpenGL/BGFX when using Diligent; optional removal of stub path.

---

## 5. Loading the scene with Diligent (including Vulkan)

### Current flow (stub)

1. **Select backend:** Use `--renderer diligent` or config `"renderer": "diligent"`. The player calls `CreateGraphicsDevice(window, width, height, RenderBackend::Diligent)` and gets a `DeviceDiligent` (stub).
2. **Scene load:** Unchanged. The same `RenderPipeline`, unified `RenderScene`, and frame allocator are used. Meshes, materials, and draw data are produced the same way.
3. **Rendering:** The pipeline calls RHI (`IDevice`, `ICommandList`) as usual. With the **stub** backend, CreateBuffer/CreateTexture return stub objects and Draw/Present are no-ops, so nothing is drawn.

So today the scene “loads” with Diligent in the sense that the same code path runs and the Diligent RHI device is used, but no real GPU work is done until Phase 2 is implemented.

### To render the scene with Diligent Vulkan (Phase 2)

1. **Device creation (Vulkan):** In `CreateDeviceDiligent` (or a new `CreateDiligentDeviceVk`), when building with Diligent and choosing Vulkan:
   - Get the Vulkan factory: `GetEngineFactoryVk()` (static link) or load the Vk DLL and import it.
   - Create device and context: `pFactoryVk->CreateDeviceAndContextsVk(EngineVkCreateInfo, &pDevice, &pContext)`.
   - Create swap chain from the window: `pFactoryVk->CreateSwapChainVk(pDevice, pContext, SwapChainDesc, Win32NativeWindow{hwnd}, &pSwapChain)`.
   - Store `IRenderDevice*`, `IDeviceContext*`, `ISwapChain*` inside `DeviceDiligent` and use them in all Create* and command-list methods.

2. **Backend choice:** Extend the API so the app can request “Diligent with Vulkan” vs “Diligent with D3D11” vs “Diligent with OpenGL” (e.g. a `DiligentBackend` enum or `--diligent-backend vulkan`). `CreateDiligentDevice(nwh, w, h, DiligentBackend::Vulkan)` would call the Vulkan factory; same for D3D11/OpenGL.

3. **RHI → Diligent in `rhi/diligent/`:** Replace stub implementations with real calls:
   - **DeviceDiligent:** `CreateBuffer` → `IRenderDevice::CreateBuffer(BufferDesc)`, same for textures, shaders, pipelines, render targets; `Present()` → `ISwapChain::Present()`.
   - **CommandListDiligent:** Hold `IDeviceContext*`; `SetRenderTarget` → `SetRenderTargets`, `BindPipeline` → `SetPipelineState`, `DrawIndexed` → `Draw(DRAW_ATTRIBS)`, etc. Use DiligentEnums to map RHITypes (format, topology, blend state) to Diligent types.
   - **Buffers/Textures/Pipelines:** Store the corresponding Diligent handle and delegate Map/Unmap, GetWidth/GetHeight, etc.

4. **Shaders:** Current passes use GLSL and RHI `CreateShader(source)`. For Diligent Vulkan you can (a) compile GLSL to SPIR-V and load with Diligent’s shader API, or (b) use HLSL and Diligent’s HLSL→SPIR-V path. Pipeline creation then uses `IRenderDevice::CreateGraphicsPipelineState(GraphicsPipelineStateCreateInfo)` with the correct vertex layout and RTV/DSV formats.

5. **One frame:** Once device and swap chain exist and at least one pipeline (e.g. fullscreen quad or a simple triangle) is implemented end-to-end, the existing scene submission will start producing real draws when the pipeline calls the same RHI API backed by Diligent Vulkan.

Summary: **Scene loading is already shared.** To “load the scene with Diligent Vulkan” you keep using `--renderer diligent` and implement Phase 2 so that `DeviceDiligent` and `CommandListDiligent` use the real Diligent Vulkan factory and context; then the same scene will render via Vulkan.

---

## 6. Shader Strategy

- **Input:** Prefer single source (HLSL or GLSL). Diligent supports HLSL across backends and GLSL for OpenGL.
- **Build:** Use Diligent’s shader compiler or platform toolchain to produce bytecode/SPIR-V for the chosen backend.
- **Runtime:** ShaderDiligent loads via Diligent shader API; PipelineDiligent builds pipeline state from shaders + layout + RHI state.

---

## 7. Success Criteria

- Render module remains backend-agnostic; no changes to pipeline/passes when adding Diligent.
- Diligent is an optional RHI backend: when selected and built, app runs with Diligent; when not built, selection falls back (e.g. to OpenGL) or stub runs.
- Stub backend allows compilation and selection without requiring Diligent Engine source.
- Document history and status updated in this plan and in `RHI_And_Graphics_Backend_Plan.md` when Diligent is production-ready.

---

## 8. Document History

- **Created:** RHI Diligent Integration Plan; Phase 0 stub backend; optional build; phased implementation (0–3).
