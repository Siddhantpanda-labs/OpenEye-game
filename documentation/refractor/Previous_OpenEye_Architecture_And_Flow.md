# OpenEye Architecture & Flow

Short reference for the current engine layout and data flow. Used as baseline for visual/rendering refractor (forward, deferred, PBR) while preserving this architecture.

---

## 1. Layer Overview

```
┌─────────────────────────────────────────────────────────────┐
│  Apps (client)                                               │
│  app/player, app/editor, app/test                            │
│  Use only api/ headers. No direct eyelib/ includes.          │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────────┐
│  API (api/) — stable, PIMPL, ABI-friendly                    │
│  Public types: RenderPipeline, RenderScene, Mesh, Material,  │
│  World, Observation, AssetRegistry, Window, etc.              │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────────┐
│  Engine library (eyelib/) — implementation                   │
│  render/, world/, asset/, animation/, physics/, platform/, … │
│  RHI under eyelib/render/rhi/ (OpenGL, optional Diligent)     │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────────┐
│  Backends / thirdparty                                        │
│  OpenGL (GLAD, GLFW), Diligent (optional), Bullet, OpenAL, …  │
└─────────────────────────────────────────────────────────────┘
```

- **Rule:** Apps include `api/` only. Implementation lives in `eyelib/`; refactor there without changing API surface.

---

## 2. Repo Layout (relevant parts)

| Path | Role |
|------|------|
| `EngineSource/api/` | Public API: render, world, scene, asset, animation, physics, platform, core, … |
| `EngineSource/eyelib/` | Engine implementation; mirrors api domains (render, world, asset, …) |
| `EngineSource/eyelib/render/` | Rendering: RHI, framegraph, passes, resources (Mesh, Shader, Material, Texture) |
| `EngineSource/eyelib/render/rhi/` | Render Hardware Interface: `IDevice`, `ICommandList`, buffers, textures, pipelines; backends: OpenGL, (Diligent) |
| `EngineSource/app/player/` | Runtime player: GameRuntime, WorldRenderBridge, uses api only (except a few internal caches) |
| `EngineSource/app/editor/` | Editor; uses api; captures snapshot then builds scene for scene/game view |
| `EngineSource/assets/` | Shaders, default materials (e.g. `shaders/opengl/`) |
| `Engine/` | Build output: bin/, lib/ |

---

## 3. Render Flow (frame)

1. **World (ECS)**  
   - `World::Update()` (or editor tick).  
   - Optional: `World::FlushTransforms()` so world matrices are up to date.

2. **Snapshot**  
   - `World::CaptureRenderSnapshot(snapshot)` — fills `RenderSnapshot` (renderables, mesh renderers, cameras, lights, XR cameras, transforms).  
   - One capture per frame. Player can use a callback (e.g. after PreRender) to capture and set frame snapshot.

3. **Scene build (app side)**  
   - `WorldRenderBridge::BuildScene(world, options, animationBridge, assetRegistry, scene, frameAlloc)`  
     - Registers capture callback and calls `CaptureRenderSnapshot`; then `BuildScene(snapshot, ...)`.  
   - Or: `BuildScene(snapshot, ...)` / `BuildScene(observation, ...)` with existing snapshot or observation.  
   - Fills `api::RenderScene`: camera, directional light, skybox, opaque/transparent drawables, point/spot lights.  
   - Resolves meshes/materials (asset registry, LOD, primitives); fills `api::Drawable` arrays.

4. **Render (API)**  
   - `api::RenderPipeline::Render(scene)`  
     - Builds `ApiRenderSceneView(scene)` implementing `eyelib::render::IRenderSceneView` (no full-scene copy).  
     - Calls `eyelib::render::RenderPipeline::Render(view)`.

5. **Pipeline (eyelib)**  
   - Mode: **Forward**, **Deferred**, or **ForwardPlus**.  
   - Uses **frame graph**: pass DAG, transient textures, barriers.  
   - **Deferred:** GpuCull → Shadow → GBuffer → Copy depth → Lighting → Transparency → Skybox → Post.  
   - **Forward:** GpuCull → Shadow → Forward Opaque → Transparency → Skybox → Post.  
   - Passes use `IRenderSceneView` (GetCamera, GetLight, ForEachOpaqueDrawable, …).  
   - All GPU work via **RHI** (`IDevice`, `ICommandList`); backend = OpenGL (or Diligent if enabled).

6. **Present**  
   - `RenderPipeline::BlitToScreen()` (or `BlitOutputTo(RenderTarget)` for XR eye targets).  
   - Optional: `Finish()`, fence before swap.

So: **World → Snapshot → BuildScene → api::RenderScene → ApiRenderSceneView → eyelib RenderPipeline (frame graph + RHI) → backends.**

---

## 4. Key Types

- **api::RenderPipeline** — PIMPL over `eyelib::render::RenderPipeline`. Init from Window or shared `RenderDevice`; `SetMode(Forward|Deferred|ForwardPlus)`; `Render(scene)`; `BlitToScreen()` / `BlitOutputTo(target)`.
- **api::RenderScene** — Camera, light, skybox, opaque/transparent drawable arrays, point/spot lights. Filled by app from snapshot/observation.
- **ApiRenderSceneView** — Wraps `api::RenderScene` and implements `eyelib::render::IRenderSceneView` so the pipeline only sees a read-only view (zero-copy).
- **RenderDevice** — Opaque RHI device handle; from `RenderPipeline::GetDevice()`; passed to `FrameGraph::Compile(device)`, texture loaders, asset loaders.
- **World::RenderSnapshot** — One-frame copy of render-relevant state (renderables, mesh renderers, cameras, lights, transforms). Filled by `CaptureRenderSnapshot(out)`.

---

## 5. RHI (eyelib/render/rhi)

- **IDevice** — Create buffers, textures, shaders, pipelines, render targets; get command list; submit; fences.  
- **ICommandList** — Bind pipeline, buffers, descriptor sets; draw/dispatch; set render target, viewport, clear; barriers.  
- **Backends:** `OpenGL/` (DeviceGL, CommandListGL, …), optional `diligent/`.  
- Device creation: `CreateOpenGLDevice()` (context current) or `CreateGraphicsDevice(makeContextCurrent)`; optional `CreateDiligentDevice(nwh, w, h)`.  
- Resources (mesh, texture, shader, material) are created with an explicit `IDevice*` (or `api::RenderDevice`); no global device.

---

## 6. Where to Change for Visual Refractor

- **Pipelines / passes:** `eyelib/render/` — RenderPipeline, framegraph, passes (GBufferPass, ShadowPass, LightingPass, etc.).  
- **PBR / materials / lighting:** Same: eyelib render resources and passes; expose any new knobs via `api/render` (e.g. material/light APIs).  
- **RHI:** Keep backend-agnostic; add or extend interfaces in `eyelib/render/rhi/` and implement in OpenGL (and Diligent if used).  
- **API:** Add or extend only what clients need (e.g. new pipeline options, material/light params); keep PIMPL and stable types in `api/render`.  
- **Apps:** Continue using only `api/`; no new direct `eyelib/` use (except existing internal caches if any).

---

## 7. References

- `documentation/intro.md` — Project intent, API-first rules, world kernel.  
- `EngineSource/eyelib/render/RENDER_MODULE.md` — Render module layout, RHI, frame graph, scene view, passes.  
- `EngineSource/eyelib/world/WORLD_MODULE.md` — World state, transform system, commands, observation, CaptureRenderSnapshot.
