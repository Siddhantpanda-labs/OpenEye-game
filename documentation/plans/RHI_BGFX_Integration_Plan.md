# RHI BGFX Backend Integration Plan

**Goal:** Replace the custom OpenGL and Vulkan RHI backends with a single **bgfx-backed RHI implementation**. The render module (pipeline, passes, RHIUtils) stays unchanged; only the code under the RHI hood switches to bgfx. Integration is **minimal and flexible**: we align with bgfx’s design where it makes sense instead of forcing a strict 1:1 RHI↔bgfx mapping, so the result is maintainable.

**Motivation (threading):** With the current OpenGL backend, the GL context lives only on the render thread. That has caused threading issues and flickering. Bgfx is built for multithreading: **thread-safe resource API** (create/destroy/update from any thread), **encoder API** (multiple threads can record work via `bgfx::begin()`), and a clear **API thread vs render thread** model. Using bgfx under the RHI should eliminate context ownership problems and stabilize rendering when the game thread and render thread are both active.

**Scope:** `eyelib/render/rhi/` (add bgfx backend, eventually remove OpenGL/Vulkan), `eyelib/platform/` (device creation: init bgfx instead of GL/Vulkan). **Unchanged:** `api/render/`, render module (RenderPipeline, passes, RHIUtils, FrameGraph, resources), game/render thread responsibilities, unified RenderScene and frame allocator.

**References:** `RHI_And_Graphics_Backend_Plan.md` (existing RHI design and interfaces); `Render_Thread_Unified_Scene_Plan.md` (thread model).

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
│  RHI Utils                                                       │  ← UNCHANGED
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  RHI (same interfaces: IDevice, ICommandList, IBuffer, …)        │  ← UNCHANGED
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  RHI::BGFX (single backend)                                      │  ← NEW
│  DeviceBGFX, CommandListBGFX, BufferBGFX, TextureBGFX,           │
│  ShaderBGFX, PipelineBGFX, RenderTargetBGFX, QueryPoolBGFX       │
│  → bgfx::init, bgfx::Encoder, bgfx::create*, bgfx::set*, submit  │
└─────────────────────────────────────────────────────────────────┘
```

- **Render module:** No code changes; it continues to use `ICommandList*`, RHI resources, and RHIUtils.
- **RHI interfaces:** No redesign; we implement them once with bgfx. Flexible mapping inside the backend (e.g. pipeline = program + state; command list = encoder usage per frame).

---

## 2. Why BGFX Helps With Threading

| Current (OpenGL) issue | Bgfx behavior |
|------------------------|----------------|
| GL context must be current on one thread; all GL calls from that thread. | **API thread** (init) and **render thread** (renderFrame). Resource create/destroy/update are **thread-safe** (mutex). |
| Game thread cannot create resources without context tricks. | Game thread can call `bgfx::create*` / `destroy*` / `update*` safely. |
| Flickering / races if work is submitted or bound from wrong thread. | **Encoders** from `bgfx::begin()`: multiple threads can record (different views); submission happens on render thread via `bgfx::renderFrame()`. Clear ownership. |

We keep **game thread** building the scene/snapshot and **render thread** owning frame submission; the RHI bgfx backend will use bgfx’s thread-safe APIs and encoder model so that resource creation and submission are well-defined and avoid the previous GL context issues.

---

## 3. Dependencies

| Dependency | Role | Integration |
|------------|------|-------------|
| **bgfx.cmake** | CMake build for bgfx + bx + bimg (pr0g/bgfx.cmake) | **Manual:** clone into `thirdparty/bgfx.cmake`, then `git submodule update --init --recursive`. |
| **bx** | Base library; required by bgfx | Submodule of bgfx.cmake (`thirdparty/bgfx.cmake/bx/`). |
| **bimg** | Image loading; required by bgfx | Submodule of bgfx.cmake (`thirdparty/bgfx.cmake/bimg/`). |
| **bgfx** | Cross-platform rendering | Submodule of bgfx.cmake (`thirdparty/bgfx.cmake/bgfx/`). |

- **Setup:** See `thirdparty/README_BGFX.md`. No FetchContent; manual clone avoids clone/remove and lock issues on Windows.
- Build: `add_subdirectory(thirdparty/bgfx.cmake)` with options (no examples, static lib, multithreaded).
- Include only bgfx headers inside `rhi/bgfx/` (and platform device creation), never in the rest of the render module.

---

## 4. RHI → BGFX Mapping (Flexible)

| RHI concept | Bgfx mapping (pragmatic) |
|-------------|---------------------------|
| **IDevice** | `bgfx::init()` + renderer type; create/destroy map to `bgfx::create*` / `destroy*`. Present → `bgfx::frame()` / swap. |
| **ICommandList** | One encoder per frame (or per submit). `Begin`/`End` → encoder scope; all `Set*`/`Bind*`/`Draw*` map to `bgfx::set*` / `bgfx::submit`. No need to emulate a full command buffer; follow bgfx’s frame/encoder model. |
| **IBuffer** | `bgfx::VertexBuffer`, `bgfx::IndexBuffer`, or dynamic variants. Uniform/storage as appropriate (e.g. uniform in constant buffer / dynamic VB). |
| **ITexture** | `bgfx::createTexture2D` (and cube/3D/array as needed). Format from RHITypes → bgfx format enum. |
| **IShader** | `bgfx::Shader` from compiled shader (bgfx shader format). See §7 Shader strategy. |
| **IPipeline** | No single “pipeline” in bgfx. Backend stores **program** (vertex + fragment shaders) + **state** (blend, depth, raster) + **vertex layout**. `BindPipeline` applies all three. |
| **IRenderTarget** | `bgfx::FrameBuffer` (or backbuffer + view). MRT = frame buffer with multiple color attachments where supported. |
| **IQueryPool** | `bgfx::OcclusionQuery` or timestamp API if available; else stub for first pass. |
| **Barriers** | Bgfx manages most transitions. Map RHI barrier calls to bgfx resource/set usage or no-ops where bgfx handles it; add explicit barriers only if bgfx exposes them and we need them. |
| **BlitToDefaultFramebuffer / Blit** | Use bgfx blit or fullscreen pass to backbuffer / between frame buffers as needed. |

We do **not** extend the RHI API to mirror every bgfx feature. We implement the existing RHI interface in a bgfx-friendly way inside `rhi/bgfx/` only.

---

## 5. Folder Structure (Additions Under eyelib/render/rhi/)

```
eyelib/render/rhi/
├── RHI.h, RHITypes.h, RHIUtils.h/cpp     # UNCHANGED
├── CreateDevice.h / .cpp                 # Extend: add CreateBGFXDevice(); platform chooses backend
├── bgfx/                                  # NEW
│   ├── DeviceBGFX.h / .cpp
│   ├── CommandListBGFX.h / .cpp
│   ├── BufferBGFX.h / .cpp
│   ├── TextureBGFX.h / .cpp
│   ├── ShaderBGFX.h / .cpp
│   ├── PipelineBGFX.h / .cpp
│   ├── RenderTargetBGFX.h / .cpp
│   ├── QueryPoolBGFX.h / .cpp
│   └── BGFXEnums.h / .cpp                 # RHITypes ↔ bgfx format/state enums
├── OpenGL/                                # Keep until bgfx path is default; then remove
└── Vulkan/                                # Keep until bgfx path is default; then remove
```

- Only `rhi/bgfx/` and platform (and optionally a small `CreateDevice` extension) include bgfx headers. Rest of eyelib stays backend-agnostic.

---

## 6. Phased Implementation

### Phase 0 — Add bgfx and build (no RHI yet)

- Add bgfx, bx, bimg to the repo (submodule or vendored under `thirdparty/`).
- Add CMake (or GENie + wrapper) so that bgfx builds as a library; link it into a small test executable that calls `bgfx::init` and `bgfx::shutdown` (e.g. empty frame). No RHI yet.
- **Exit criterion:** bgfx builds and one empty frame runs (e.g. OpenGL or Vulkan backend).

### Phase 1 — RHI BGFX backend skeleton

- Create `rhi/bgfx/` with stub implementations of all RHI interfaces (DeviceBGFX, CommandListBGFX, BufferBGFX, TextureBGFX, ShaderBGFX, PipelineBGFX, RenderTargetBGFX, QueryPoolBGFX).
- DeviceBGFX: init bgfx in constructor (using platform window/surface), shutdown in destructor; GetImmediateCommandList returns a CommandListBGFX that records into the current frame’s encoder.
- Implement minimal Create* / Destroy* that delegate to bgfx; implement minimal CommandListBGFX (e.g. SetRenderTarget, Clear, BindPipeline, DrawIndexed) so one draw works.
- **Exit criterion:** RHI device creation (CreateBGFXDevice) works; one triangle or fullscreen quad renders through the RHI using the bgfx backend.

### Phase 2 — Full RHI interface on bgfx

- Implement all IDevice and ICommandList methods in the bgfx backend (buffers, textures, shaders, pipelines, render targets, blit, present, barriers where needed).
- Implement IQueryPool for timestamps/occlusion if required by Profiler; otherwise stub.
- Add BGFXEnums (and possibly small helpers) for format/state conversion.
- **Exit criterion:** All current render paths that go through RHI can run on the bgfx backend (forward and deferred, shadows, post-process, etc.) without regressions.

### Phase 3 — Shaders and assets

- Define shader workflow: build bgfx shaders from existing GLSL (or generate) and load via ShaderBGFX. Prefer a single pipeline (e.g. shaderc or bgfx’s shader compiler) so we don’t maintain two completely different shader trees long-term.
- Ensure all passes and materials use RHI shader/pipeline creation; no leftover GL-specific paths when bgfx is active.
- **Exit criterion:** All in-engine shaders build and run through bgfx; no GL shader path when backend is bgfx.

### Phase 4 — Platform and device selection

- Platform (e.g. `CreateGraphicsDevice` or equivalent) creates the RHI device: when “bgfx” is selected, call CreateBGFXDevice(window/surface) instead of CreateOpenGLDevice / Vulkan. Render module still receives `IDevice*`; it does not know the backend.
- Optional: runtime or compile-time switch (e.g. env var or config) to choose bgfx vs OpenGL until migration is complete.
- **Exit criterion:** App runs with bgfx as the only backend (or as default); game + render thread run without GL context issues and without flickering.

### Phase 5 — Remove old backends and clean up

- Remove or conditionally exclude `rhi/OpenGL/` and `rhi/Vulkan/` from the build when bgfx is the only backend.
- Remove GL/Vulkan-specific code from platform (e.g. glad, Vulkan loader) if no longer used.
- Update docs (this plan and RHI_And_Graphics_Backend_Plan.md) to state that the only RHI implementation is bgfx.
- **Exit criterion:** Single backend (bgfx); clean build; no dead code for GL/Vulkan RHI.

---

## 7. Shader Strategy

- **Input:** Prefer one source of truth (e.g. GLSL or a single intermediate). Avoid maintaining separate GLSL and Vulkan-only paths long-term.
- **Build:** Use bgfx’s shader compilation (e.g. `shaderc` or the bgfx toolchain) to produce shaders for the chosen backend(s). Ship compiled shaders (e.g. .bin) or compile at build time.
- **Runtime:** ShaderBGFX loads via `bgfx::createShader`; pipeline combines vertex + fragment (and compute if needed). Uniforms: use bgfx’s uniform system or map RHI SetUniform* to bgfx set*.
- **Compatibility:** Existing PBR, IBL, shadows, post-process logic stays in the render module; only the shader assets and the way they are created/loaded change for the bgfx backend.

---

## 8. Threading Alignment

- **Game thread:** Builds scene snapshot, may create/update/destroy RHI resources. With bgfx, these calls go to thread-safe `bgfx::create*` / `destroy*` / `update*` inside the backend; no GL context required.
- **Render thread:** Owns frame: get encoder (or equivalent), run all passes (they call RHI ICommandList methods), then call `bgfx::frame()` / present. Bgfx’s render thread is where `bgfx::renderFrame()` runs; we align that with our existing render thread.
- **Result:** No context “current thread” ambiguity; no flicker from cross-thread GL usage; clear ownership of submission.

---

## 9. Success Criteria

- Render module (pipeline, passes, RHIUtils) is **unchanged**; it only talks to RHI.
- A single RHI backend, **bgfx**, is implemented under the existing RHI interfaces; implementation is flexible and maintainable.
- Game thread + render thread run without GL context issues and without flickering.
- All current rendering features (forward, deferred, PBR, IBL, shadows, post-process) work through the bgfx backend.
- Old OpenGL/Vulkan RHI backends are removed; build and codebase are simplified.

---

## 10. Document History

- **Created:** RHI BGFX Integration Plan; goal to replace custom OpenGL/Vulkan with bgfx under RHI; minimal, flexible refactor; render module unchanged; threading motivation and phased implementation.
