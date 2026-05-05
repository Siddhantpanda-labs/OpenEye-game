Certainly. A **next-generation render module** is defined not by a single rendering technique but by its **architectural flexibility, scalability, and ability to integrate multiple rendering paradigms seamlessly**. Such a system governs a wide spectrum of features, spanning geometry, lighting, materials, data management, and extensibility.

Below is a comprehensive yet concise enumeration of the **features and characteristics** that define a modern, future-ready render module.

---

## 1. Architectural Characteristics

### 1.1 Modular and Feature-Oriented Design

- **Pluggable render features** (e.g., rasterization, Nanite, Gaussian splatting, ray tracing).
- **Self-contained modules** with clear ownership of resources and logic.
- Ability to enable or disable features without modifying the core pipeline.

### 1.2 Clear Separation of Concerns

- **Render System:** Lifecycle and global resource ownership.
- **Render Pipeline:** Per-frame orchestration.
- **Frame Graph:** Pass scheduling, synchronization, and resource lifetimes.
- **RHI (Render Hardware Interface):** Backend abstraction.
- **Scene Interface:** Read-only contract supplying renderable data.
- **Render Features:** Independent rendering techniques.

### 1.3 Explicit Dependency and Ownership Model

- No hidden global state.
- Interface-driven communication.
- Deterministic data flow:
**Scene → Render Features → Frame Graph → RHI → GPU**.

### 1.4 Scalability and Extensibility

- Seamless integration of new rendering paradigms.
- Support for multiple pipelines (game, editor, VR, offline).
- Plugin-friendly architecture.
- Data-oriented and GPU-driven workflows.

---

## 2. Geometry and Scene Representation

### 2.1 Traditional Rasterized Geometry

- Static and skinned meshes.
- Instancing and batching.
- Level of Detail (LOD) management.
- GPU-driven rendering and indirect draws.

### 2.2 Virtualized Geometry (Nanite-Style)

- Cluster-based hierarchical geometry.
- On-demand streaming and paging.
- GPU-driven culling and LOD selection.
- Software or compute-based rasterization.
- Integration with material and lighting systems.

### 2.3 Point-Based and Neural Representations

- **Gaussian Splatting** for radiance field rendering.
- Support for **NeRF and neural scene representations**.
- Hybrid compositing with rasterized geometry.
- Specialized visibility and shading pipelines.

### 2.4 Procedural and Volumetric Geometry

- Terrain and procedural generation.
- Signed Distance Fields (SDFs).
- Volumetric rendering (clouds, fog, smoke).

---

## 3. Lighting and Global Illumination

### 3.1 Raster-Based Lighting

- Deferred, Forward, and Forward+ shading.
- Clustered or tiled light culling.
- Physically Based Rendering (PBR).
- Image-Based Lighting (IBL).

### 3.2 Ray and Path Tracing

- Hardware-accelerated ray tracing (DXR/Vulkan RT).
- Hybrid raster + ray tracing pipelines.
- Real-time path tracing for offline-quality rendering.
- Acceleration structure management (BLAS/TLAS).

### 3.3 Global Illumination Techniques

- Screen Space Global Illumination (SSGI).
- Probe-based GI (irradiance volumes, reflection probes).
- Real-time solutions (e.g., Lumen-style systems).
- ReSTIR-based light sampling.

---

## 4. Material and Shading System

### 4.1 Physically Based Material Framework

- Energy-conserving BRDF/BSDF models.
- Support for layered and anisotropic materials.
- Clear separation between material description and shader implementation.

### 4.2 Shader Management

- Shader permutation and variant system.
- Reflection and automatic resource binding.
- Hot-reloading and caching.
- Cross-backend shader abstraction.

### 4.3 Bindless and Descriptor Management

- Bindless textures and buffers.
- Efficient descriptor allocation and reuse.
- Scalability for large scenes.

---

## 5. Frame Graph and Rendering Orchestration

### 5.1 Frame Graph Execution

- Declarative pass definitions.
- Automatic resource lifetime management.
- Memory aliasing and barrier insertion.
- Deterministic execution order.

### 5.2 Asynchronous and Parallel Execution

- Support for **async compute** and multi-queue scheduling.
- Parallel command list recording.
- GPU/CPU synchronization primitives.

### 5.3 Multi-View and Multi-Pipeline Support

- Stereo and VR rendering.
- Multiple cameras and viewports.
- Editor and game pipeline coexistence.

---

## 6. Post-Processing and Image Synthesis

### 6.1 Traditional Post-Processing

- Tone mapping and HDR management.
- Bloom, depth of field, motion blur.
- Anti-aliasing (TAA, FXAA, MSAA).

### 6.2 Neural and AI-Based Enhancements

- Super-resolution (e.g., DLSS/FSR/XeSS-like systems).
- AI denoising for ray tracing.
- Neural texture and material synthesis.
- Frame generation and interpolation.

### 6.3 Compositing and Transparency

- Order-independent transparency (OIT).
- Volumetric lighting and fog.
- Hybrid compositing of raster, ray-traced, and neural outputs.

---

## 7. Resource and Memory Management

### 7.1 GPU Resource Abstraction

- Unified handling of buffers, textures, and render targets.
- Explicit memory allocation strategies.
- Efficient streaming of large assets.

### 7.2 Virtualization and Streaming

- Virtual textures (megatextures).
- Geometry streaming (Nanite-style).
- Sparse resources and residency management.

### 7.3 Caching Systems

- Shader cache.
- Pipeline state cache.
- Descriptor and resource caches.

---

## 8. Performance and Scalability Features

### 8.1 GPU-Driven Rendering

- Indirect drawing and compute-based culling.
- Clustered/tiled light management.
- Reduced CPU-GPU synchronization.

### 8.2 Level of Detail and Visibility

- Hierarchical Z-buffer (Hi-Z) occlusion culling.
- Frustum and portal culling.
- Adaptive quality scaling.

### 8.3 Profiling and Debugging

- GPU timing and statistics.
- Frame graph visualization.
- RenderDoc and PIX integration.
- Debug overlays and entity picking.

---

## 9. Platform and Backend Support

### 9.1 Cross-Platform RHI

- Support for Vulkan, DirectX 12, Metal, and OpenGL.
- Backend capability abstraction.
- Consistent shader and resource interfaces.

### 9.2 Scalability Across Hardware

- Mobile to high-end desktop and console support.
- Dynamic quality and feature scaling.
- Graceful fallback mechanisms.

---

## 10. Editor and Tooling Integration

### 10.1 Editor-Specific Rendering

- Entity ID and selection buffers.
- Gizmos and debug visualization.
- Scene preview and thumbnail rendering.

### 10.2 Asset and Pipeline Tools

- Material and shader editors.
- Real-time preview and hot reload.
- Capture and replay systems.

---

## 11. Extensibility and Plugin Ecosystem

### 11.1 Feature Registration System

- Plugin-based architecture for new rendering methods.
- Runtime feature toggling.
- Versioned and backward-compatible interfaces.

### 11.2 Data-Driven Configuration

- Render pipelines defined through configuration files.
- Customizable pass sequences.
- User-defined rendering workflows.

---

## 12. Security, Determinism, and Robustness

### 12.1 Deterministic Rendering

- Stable frame execution order.
- Reproducible results for debugging and networking.

### 12.2 Robust Resource Handling

- Graceful handling of device loss.
- Validation and error reporting.
- Safe synchronization mechanisms.

---

## 13. Summary Table


| Category                | Key Features                                                           |
| ----------------------- | ---------------------------------------------------------------------- |
| **Architecture**        | Modular, feature-oriented, explicit dependencies                       |
| **Geometry**            | Raster meshes, Nanite virtual geometry, Gaussian splatting, procedural |
| **Lighting**            | Deferred/Forward+, ray tracing, path tracing, GI                       |
| **Materials**           | PBR, shader permutations, bindless resources                           |
| **Frame Graph**         | Pass orchestration, resource lifetimes, async compute                  |
| **Post-Processing**     | Tone mapping, TAA, neural super-resolution                             |
| **Resource Management** | Streaming, virtualization, caching                                     |
| **Performance**         | GPU-driven rendering, LOD, occlusion                                   |
| **Platform Support**    | Cross-platform RHI, scalability                                        |
| **Tooling**             | Editor overlays, debugging, profiling                                  |
| **Extensibility**       | Plugin-based feature system                                            |
| **Robustness**          | Determinism, device loss handling                                      |


---

## Final Perspective

A **next-generation render module** is not defined by a single rendering technique but by its **ability to orchestrate multiple paradigms within a clean, extensible, and scalable architecture**. It should seamlessly integrate rasterization, ray tracing, neural rendering, virtualized geometry, and emerging representations such as Gaussian splatting, all while maintaining clear ownership, explicit dependencies, and elegant design.

This foundation ensures that the rendering system remains **future-proof**, adaptable to industry evolution, and capable of supporting both real-time and cinematic-quality experiences.


Let me read your architecture file carefully before we dive in.Okay, I've read through your current architecture carefully. Let me think through this properly before showing you the visualization.

Here's what I observe about your current design's pain points relevant to adding next-gen features:

**What's solid in your current design:**
- Frame graph is the right foundation — barriers, aliasing, pass ordering are all captured there
- Explicit `IDevice*` everywhere (no globals) is clean
- `IRenderSceneView` as a read-only contract is good

**What will hurt when you add Nanite, path tracing, Lumen-style GI, Gaussian Splatting, NeRF:**

1. **`RenderPipeline` is a god object** — it owns resources, pass systems, caches, orchestrator, AND makes mode decisions. Nanite needs its own geometry processing budget, virtual shadow maps have their own lifetime, path tracing needs a completely different resource set. You can't bolt these onto one class cleanly.

2. **Pipeline "modes" are baked enums** — `Hybrid/Forward/ForwardPlus` is hardcoded branching. Adding a path tracing mode or a neural rendering path means more if/else chains. It doesn't compose.

3. **Post is orphaned after the graph** — tone mapping, bloom etc. run outside the frame graph. That's fine for rasterization but path tracing, NeRF output, and Gaussian splat compositing all need to participate in the same HDR accumulation and temporal stack. They can't just be appended.

4. **Geometry representation is monolithic** — `Mesh` is one thing. Nanite needs a virtual geometry pipeline (cluster DAGs, software rasterization path, hardware fallback). Gaussian splats aren't meshes at all. NeRF/neural fields are volume queries. These need a **geometry abstraction layer** so passes don't care what they're drawing.

5. **Lighting is scattered** — `SceneLighting`, `ClusteredLightGrid`, `IBL`, `ReflectionProbe`, `CascadeShadowMap` are all separate with no unified "lighting system" interface. Lumen-style dynamic GI needs to own and invalidate all of these together, as one coherent subsystem with feedback loops between them.

6. **No concept of temporal context** — DLSS/TSR/path tracing accumulation, motion vectors, history buffers need a first-class home. Right now depth copy (`kCopyDepthOcclusion`) is the only temporal artifact and it's ad-hoc.

---

Now here's the high-level ownership and flow visualization for the **next-gen design**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            APPLICATION BOUNDARY                             │
│   BeginFrame ──► UploadPhase ──► Render(SceneView) ──► EndFrame/Present    │
└─────────────────────────────┬───────────────────────────────────────────────┘
                              │  IRenderSceneView  (read-only contract, unchanged)
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         RENDER WORLD  (owns the frame)                      │
│                                                                             │
│   RenderWorld                                                               │
│   ├── owns: FrameScheduler        (what runs this frame, in what order)     │
│   ├── owns: RenderResources       (HDR, depth, GBuffer, history, temporal)  │
│   ├── owns: VisibilitySystem      (what's visible, in what representation)  │
│   ├── owns: GeometrySystem        (how geometry is submitted to GPU)        │
│   ├── owns: LightingSystem        (unified lighting, GI, shadows, IBL)      │
│   ├── owns: MaterialSystem        (materials, shaders, permutations)        │
│   └── owns: RenderFeatureRegistry (pluggable rendering features)            │
└──────────────┬──────────────────────────────────────────────────────────────┘
               │
    ┌──────────┴──────────────────────────────────────┐
    │                                                 │
    ▼                                                 ▼
┌───────────────────────┐               ┌─────────────────────────────────┐
│   FRAME SCHEDULER     │               │      RENDER FEATURE REGISTRY    │
│                       │               │                                 │
│  Owns the frame graph │               │  Features register themselves:  │
│  Knows active feature │               │  ┌─────────────────────────┐   │
│  set this frame       │               │  │ RasterFeature           │   │
│                       │               │  │  (Forward/Deferred/F+)  │   │
│  Compiles passes from │               │  ├─────────────────────────┤   │
│  active features      │               │  │ VirtualGeometryFeature  │   │
│                       │               │  │  (Nanite)               │   │
│  Manages resource     │               │  ├─────────────────────────┤   │
│  lifetimes/aliasing   │               │  │ GaussianSplatFeature    │   │
│                       │               │  ├─────────────────────────┤   │
│  Frame graph Execute  │               │  │ NeuralRadianceFeature   │   │
│  → post-graph stack   │               │  │  (NeRF)                 │   │
└───────────┬───────────┘               │  ├─────────────────────────┤   │
            │                           │  │ PathTracingFeature      │   │
            │ schedules passes from     │  ├─────────────────────────┤   │
            ▼                           │  │ LumenGIFeature          │   │
┌───────────────────────┐               │  ├─────────────────────────┤   │
│     FRAME GRAPH       │               │  │ PostStackFeature        │   │
│   (unchanged core)    │               │  │  (tone/bloom/DLSS/TSR)  │   │
│                       │               │  └─────────────────────────┘   │
│  Passes declared with │               │                                 │
│  read/write resource  │               │  Each feature:                  │
│  sets                 │               │  - declares its passes          │
│  Barrier compilation  │               │  - declares resource needs      │
│  Lifetime aliasing    │               │  - declares dependencies        │
│  Execute in order     │               │  - gets called to build/exec    │
└───────────┬───────────┘               └─────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SUBSYSTEMS  (owned by RenderWorld)                  │
│                                                                             │
│  ┌─────────────────────┐   ┌──────────────────────┐   ┌─────────────────┐  │
│  │  VISIBILITY SYSTEM  │   │   GEOMETRY SYSTEM     │   │ LIGHTING SYSTEM │  │
│  │                     │   │                       │   │                 │  │
│  │  GPU Cull           │   │  IGeometryProvider    │   │  SceneLights    │  │
│  │  Occlusion          │   │  (abstract interface) │   │  ShadowSystem   │  │
│  │  LOD selection      │   │  ├─ MeshGeometry      │   │  ├─ VSM         │  │
│  │  Cluster streaming  │   │  ├─ NaniteGeometry    │   │  ├─ Cascades    │  │
│  │  Visibility buffer  │   │  ├─ SplatGeometry     │   │  ├─ RayTraced   │  │
│  │                     │   │  └─ NeuralGeometry    │   │  IBL            │  │
│  │  Outputs:           │   │                       │   │  ReflectionMgr  │  │
│  │  DrawList per type  │   │  DrawList consumer:   │   │  GISystem       │  │
│  └──────────┬──────────┘   │  takes DrawList from  │   │  ├─ Lumen-style │  │
│             │              │  Visibility, emits     │   │  ├─ DDGI        │  │
│             └──────────────►  GPU work              │   │  └─ PathTraced  │  │
│                            └──────────────────────┘   └─────────────────┘  │
│                                                                             │
│  ┌──────────────────────────────────┐   ┌──────────────────────────────┐   │
│  │       TEMPORAL SYSTEM            │   │      MATERIAL SYSTEM         │   │
│  │                                  │   │                              │   │
│  │  History buffer manager          │   │  Material / Instance         │   │
│  │  Motion vector pass              │   │  MaterialCompiler            │   │
│  │  TAA / DLSS / TSR / XeSS         │   │  ShaderCache / PipelineCache │   │
│  │  Accumulation budget             │   │  Permutation system          │   │
│  │  Jitter sequence                 │   │  ├─ Raster permutations      │   │
│  │  Denoiser integration            │   │  ├─ RT hit groups            │   │
│  │  (path trace / AO / GI)          │   │  └─ Compute variants         │   │
│  └──────────────────────────────────┘   └──────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                               RHI LAYER                                     │
│   IDevice  ·  ICommandList  ·  Buffers/Textures  ·  Pipelines              │
│   Barriers  ·  Descriptors  ·  Bindless heap  ·  RT acceleration structs   │
│   Backends: Vulkan | (OpenGL legacy) | (Diligent optional)                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

**The key architectural ideas in that diagram:**

**RenderWorld replaces RenderPipeline as the god object**, but it delegates fully — it owns subsystems, not their logic.

**RenderFeatureRegistry is the composability answer** — instead of `if (mode == ForwardPlus)`, features self-register. Nanite is a feature. Gaussian splatting is a feature. Path tracing is a feature. The scheduler asks each active feature "what passes do you need this frame?" and builds the graph from that.

**IGeometryProvider is the representation boundary** — passes don't care if they're drawing triangle meshes, Nanite clusters, splats, or neural fields. They consume a draw list from Visibility and hand it to the appropriate provider.

**LightingSystem is unified** — shadows, IBL, GI, and reflections are one coherent system with feedback. Lumen-style GI can invalidate probe caches. VSM and Cascades are strategies under ShadowSystem, not separate singletons.

**TemporalSystem is first-class** — not an afterthought. Every feature that produces noisy output (path tracing, DDGI, AO) registers its buffers with TemporalSystem. Denoising and accumulation are coordinated there.

---


Yes — this architecture is specifically designed so that each advanced system lives in its own domain with clear boundaries. The feature registry pattern means you're never modifying core orchestration to add something new, and the subsystem ownership means Nanite, path tracing, and neural rendering never step on each other's data.

Here's a comprehensive map of modern rendering techniques, organized by domain:

---

**GEOMETRY REPRESENTATION**
- Nanite — virtual geometry, cluster DAGs, software rasterizer fallback
- Meshlets — mesh shader pipeline, cluster-based culling
- Displacement / tessellation — hardware and compute-based
- Gaussian Splatting (3DGS) — point-cloud radiance, alpha compositing
- NeRF / Instant-NGP — neural radiance fields, hash grid encoding
- Signed Distance Fields (SDF) — ray marching, soft shadows, AO
- Sparse Voxel Octrees (SVO) — volumetric geometry
- BVH / acceleration structures — for ray tracing and GPU queries

---

**VISIBILITY & CULLING**
- GPU-driven rendering — indirect draw, compute cull, no CPU bottleneck
- Two-pass occlusion culling — HZB reprojection
- Cluster / meshlet culling — cone + sphere + frustum per cluster
- Visibility buffer (deferred texturing) — material resolve from triangle ID
- Hierarchical Z-buffer (HiZ)
- Software rasterizer — Nanite's small triangle fast path
- Temporal reprojection culling

---

**GLOBAL ILLUMINATION**
- Lumen (Unreal style) — surface cache, radiance cache, screen traces + hw RT fallback
- DDGI — dynamic diffuse GI with irradiance probes
- ReSTIR GI — reservoir-based spatiotemporal resampling
- Path tracing — offline-quality, progressive accumulation
- Radiance Cascades — new screen-space GI (Shahriari 2024)
- VXGI — voxel cone tracing
- SSGI / HBAO+ — screen space approximations
- Spherical Harmonics baking — static/lightmap GI

---

**SHADOWS**
- Virtual Shadow Maps (VSM) — Nanite-compatible, page-based
- Ray traced shadows — hard and soft
- Cascaded Shadow Maps (CSM) — your current system
- PCSS — percentage closer soft shadows
- SDF shadows — ray march signed distance fields
- Moment Shadow Maps
- Stochastic shadow maps — for path tracing integration

---

**REFLECTIONS**
- Lumen reflections — multi-bounce, surface cache fed
- Ray traced reflections — hardware RT
- Screen Space Reflections (SSR) — hierarchical ray march
- ReSTIR reflections — reservoir resampling for specular
- Reflection capture / probes — static baked
- Planar reflections — mirrors and water

---

**LIGHTING MODELS**
- Clustered / tiled deferred — your Forward+
- Physically Based Rendering (PBR) — GGX, multi-scattering BRDF
- Subsurface Scattering (SSS) — separable, transmittance
- Hair / strand shading — Marschner, Kajiya-Kay
- Cloth shading — Ashikhmin, Charlie
- Eye shading — cornea refraction, limbal ring
- Thin film interference
- Heterogeneous participating media — volumetric fog, clouds

---

**ATMOSPHERE & ENVIRONMENT**
- Physically based sky — Bruneton, Hillaire atmosphere model
- Volumetric clouds — ray marching, temporal reprojection
- Volumetric fog / participating media
- Ocean / water rendering — FFT waves, subsurface
- Terrain rendering — virtual texturing, heightfield, mesh

---

**ANTI-ALIASING & UPSCALING**
- DLSS 3/4 — neural upscaling + frame generation
- FSR 3 — AMD spatial + temporal upscaling
- XeSS — Intel neural upscaling
- TSR (Temporal Super Resolution) — Unreal's own
- TAA — temporal anti-aliasing baseline
- MSAA — multisampling, still relevant for forward
- SMAA — morphological

---

**POST PROCESSING**
- Tone mapping — ACES, AgX, filmic curves
- Bloom — physically based (dual-kawase, FFT)
- Depth of field — CoC, gather, scatter, bokeh
- Motion blur — per-object, per-bone velocity
- Chromatic aberration
- Lens flares — physically based
- Film grain / noise
- Color grading — LUT-based
- Eye adaptation / auto-exposure
- Vignette, distortion

---

**DENOISING**
- SVGF — spatiotemporal variance-guided filtering
- ReLAX — ReSTIR-compatible denoiser (NRD)
- DLSS Ray Reconstruction — neural denoising
- Temporal accumulation — path trace sample blending
- Firefly suppression

---

**NEURAL / AI RENDERING**
- Neural radiance caching — replace expensive GI with network inference
- Neural materials — learned BRDF representation
- Neural LOD — network-inferred geometry detail
- Neural denoising — all the above denoisers
- Generative texture synthesis — runtime material variation
- Gaussian Splatting refinement networks

---

**COMPUTE & GPU-DRIVEN**
- Async compute — overlap shadow/cull with graphics
- Work graphs (DX12 / Vulkan) — GPU-driven pass scheduling
- Mesh shaders — amplification + mesh stage
- Task/amplification shaders — Nanite cluster selection
- Bindless resources — descriptor heap indexing
- GPU scene — all transforms/materials on GPU, no per-draw CPU calls
- Multi-draw indirect / draw stream

---

**RAY TRACING**
- Hardware RT (DXR / Vulkan RT) — BVH traversal
- Inline ray tracing — trace from any shader stage
- RTAO — ray traced ambient occlusion
- ReSTIR DI — direct illumination resampling
- Hybrid ray tracing — raster primary, RT for secondary effects

---

**SPECIAL / EMERGING**
- Procedural generation on GPU — geometry shaders, terrain, foliage
- Simulation rendering — fluid, cloth, particle GPU sims
- Mixed reality compositing — depth-aware AR blending
- Variable Rate Shading (VRS) — coarse shading in low-detail regions
- Render graph driven multi-GPU — explicit GPU affinity
- Compressed textures — BC7, ASTC, runtime transcoding
- Sparse textures / virtual texturing — streaming pages on demand
- Micro-polygon rendering — sub-pixel geometry handling

---

That's roughly the full landscape as of today. The architecture we discussed can hold all of this because:

- Each row in "GEOMETRY" is an `IGeometryProvider` implementation
- Each row in "GI" is a strategy inside `LightingSystem`
- Each row in "SHADOWS" is a strategy inside `ShadowSystem`
- Each row in "UPSCALING / DENOISING" lives in `TemporalSystem`
- Everything else is a `RenderFeature` that self-registers and declares its passes

Nothing in core orchestration changes when you add any of these. That's the goal.


Yes, RHI stays exactly as-is. It's already well-designed — `IDevice`, `ICommandList`, the Vulkan backend subsystems, barriers, bindless heap, all of it. That layer is solid and backend-agnostic. We're only redesigning everything **above** the RHI.

Let me read your current structure one more time carefully before proposing the new layout, so I map old → new accurately.Good. Now I have the full picture. Here's the new directory layout — every folder has one clear job, nothing more.

---

```
eyelib/rendering/
│
│   ── UNCHANGED ──────────────────────────────────────────────────────
├── rhi/                          ← KEEP AS-IS. Not touched.
│   ├── Vulkan/                   ← KEEP AS-IS.
│   ├── OpenGL/                   ← KEEP AS-IS.
│   ├── diligent/                 ← KEEP AS-IS.
│   ├── RHI.h
│   ├── RHITypes.h
│   └── ...
│
│   ── FRAME CONTRACT (what the app/scene hands to the renderer) ───────
├── scene/
│   ├── IRenderSceneView.h        ← moved from root (was there before)
│   ├── RenderTypes.h             ← moved from root
│   ├── CameraView.h              ← split out of RenderTypes
│   ├── CameraConventions.h       ← moved from root
│   ├── DrawList.h                ← NEW — typed visibility results
│   └── RenderSceneView.cpp
│
│   ── CORE FRAME INFRASTRUCTURE ─────────────────────────────────────
├── core/
│   ├── RenderWorld.h/.cpp        ← NEW — replaces RenderPipeline as owner
│   ├── FrameContext.h            ← was FrameRenderContext.h
│   ├── RenderContext.h           ← moved from root (unchanged concept)
│   ├── RenderResources.h/.cpp    ← NEW — owns HDR/depth/GBuffer/history RTs
│   ├── RenderCapabilities.h      ← NEW — what the device/backend supports
│   ├── RenderSettings.h          ← NEW — quality levels, feature toggles
│   ├── PassPushConstants.h       ← moved from root
│   ├── FullscreenQuad.h/.cpp     ← moved from root
│   └── GpuProfiler.h/.cpp        ← was GpuProfilerRHI
│
│   ── FRAME GRAPH (keep, clean up slightly) ─────────────────────────
├── framegraph/
│   ├── FrameGraph.h/.cpp         ← KEEP — graph API, compile, execute
│   ├── FrameGraphPass.h          ← NEW — IFrameGraphPass base (was inline)
│   ├── FrameGraphResource.h      ← NEW — resource handle declarations
│   ├── FrameGraphPassNames.h     ← moved from renderer/graph/
│   └── FrameGraphBlackboard.h   ← NEW — typed resource sharing between passes
│
│   ── FRAME SCHEDULING (who runs what and when) ─────────────────────
├── scheduler/
│   ├── FrameScheduler.h/.cpp     ← NEW — replaces RenderGraphOrchestrator
│   ├── RenderFeature.h           ← NEW — IRenderFeature interface
│   ├── RenderFeatureRegistry.h/.cpp ← NEW — features self-register here
│   ├── FeatureSet.h              ← NEW — active feature combination this frame
│   └── SchedulerContext.h        ← NEW — what scheduler passes to features
│
│   ── VISIBILITY & CULLING ───────────────────────────────────────────
├── visibility/
│   ├── VisibilitySystem.h/.cpp   ← NEW — owns all culling, outputs DrawLists
│   ├── GpuCuller.h/.cpp          ← was GpuCullPassSystem + GpuCullPass
│   ├── HzbGenerator.h/.cpp       ← NEW — hierarchical Z-buffer
│   ├── OcclusionCuller.h/.cpp    ← NEW — two-pass occlusion
│   ├── ClusterCuller.h/.cpp      ← NEW — meshlet/cluster cone+sphere cull
│   ├── FrustumCuller.h/.cpp      ← was CullingSystem.h (CPU side)
│   └── VisibilityBuffer.h/.cpp   ← NEW — visibility buffer / deferred texturing
│
│   ── GEOMETRY PROVIDERS (what gets drawn, in what representation) ───
├── geometry/
│   ├── IGeometryProvider.h       ← NEW — pure interface all providers implement
│   ├── MeshGeometry.h/.cpp       ← was Mesh + BatchBuilder + DrawBatch
│   ├── MeshDescriptor.h          ← moved from root
│   ├── MeshDescriptorUtils.h/.cpp← moved from root
│   ├── MeshOptimizer.h/.cpp      ← moved from resources/
│   ├── InstancedGeometry.h/.cpp  ← was InstancedRenderer
│   ├── NaniteGeometry.h/.cpp     ← NEW — virtual geometry provider
│   ├── NaniteClusterDAG.h/.cpp   ← NEW — cluster hierarchy, streaming
│   ├── NaniteSoftwareRaster.h/.cpp ← NEW — small triangle fast path
│   ├── GaussianSplatGeometry.h/.cpp ← NEW — 3DGS point cloud provider
│   ├── NeuralGeometry.h/.cpp     ← NEW — NeRF/neural field provider (stub)
│   └── SdfGeometry.h/.cpp        ← NEW — SDF volume provider
│
│   ── MATERIALS & SHADERS ────────────────────────────────────────────
├── materials/
│   ├── Material.h/.cpp           ← KEEP
│   ├── MaterialInstance.h/.cpp   ← KEEP
│   ├── MaterialDescriptor.h      ← KEEP
│   ├── MaterialBuilder.h/.cpp    ← KEEP
│   ├── MaterialCompiler.h/.cpp   ← KEEP
│   ├── MaterialShader.h/.cpp     ← KEEP
│   └── MaterialDomain.h          ← NEW — Opaque/Transparent/RT/Compute domains
│
├── shaders/
│   ├── ShaderKey.h               ← moved from root
│   ├── ShaderDescriptor.h        ← moved from root
│   ├── ShaderLoader.h/.cpp       ← moved from root
│   ├── ShaderSourceLoader.h/.cpp ← moved from root
│   ├── ShaderReflection.h/.cpp   ← moved from root
│   ├── ShaderCache.h/.cpp        ← moved from root
│   ├── ShaderWatcher.h/.cpp      ← moved from root
│   ├── ShaderPathResolve.h       ← moved from root
│   ├── ShaderTemplateName.h      ← moved from root
│   ├── PipelineCache.h/.cpp      ← moved from root
│   └── ShaderPermutationKey.h    ← NEW — typed permutation flags (RT/raster/compute)
│
│   ── LIGHTING (unified: direct, indirect, shadows, IBL) ─────────────
├── lighting/
│   ├── LightingSystem.h/.cpp     ← NEW — owns and coordinates all below
│   ├── SceneLighting.h/.cpp      ← KEEP — fills GPU light buffers
│   ├── LightCuller.h/.cpp        ← was LightCullPassSystem + LightCullPass
│   ├── ClusteredLightGrid.h/.cpp ← KEEP
│   ├── TiledLightGrid.h/.cpp     ← KEEP (add header)
│   │
│   ├── shadows/
│   │   ├── IShadowStrategy.h     ← NEW — interface for all shadow methods
│   │   ├── CascadeShadowMap.h/.cpp ← KEEP — CSM strategy
│   │   ├── VirtualShadowMap.h/.cpp ← NEW — VSM (Nanite-compatible)
│   │   ├── RayTracedShadows.h/.cpp ← NEW — RT shadow strategy
│   │   └── ShadowAtlas.h/.cpp    ← NEW — shared shadow atlas manager
│   │
│   ├── gi/
│   │   ├── IGlobalIllumination.h ← NEW — GI strategy interface
│   │   ├── LumenGI.h/.cpp        ← NEW — surface cache + radiance cache
│   │   ├── DDGI.h/.cpp           ← NEW — dynamic diffuse GI probes
│   │   ├── ReSTIRGI.h/.cpp       ← NEW — reservoir GI
│   │   ├── PathTracedGI.h/.cpp   ← NEW — path trace GI accumulation
│   │   ├── RadianceCascades.h/.cpp ← NEW
│   │   └── VXGI.h/.cpp           ← NEW — voxel cone tracing
│   │
│   ├── reflections/
│   │   ├── IReflectionStrategy.h ← NEW — interface
│   │   ├── ScreenSpaceReflections.h/.cpp ← NEW (was loose)
│   │   ├── RayTracedReflections.h/.cpp ← NEW
│   │   ├── ReSTIRReflections.h/.cpp ← NEW
│   │   ├── ReflectionProbe.h/.cpp← moved from reflection/
│   │   └── ReflectionProbeManager.h/.cpp ← moved from reflection/
│   │
│   └── ibl/
│       └── IBL.h/.cpp            ← moved from ibl/
│
│   ── RENDER FEATURES (self-contained, register into scheduler) ──────
├── features/
│   │
│   ├── raster/
│   │   ├── RasterFeature.h/.cpp  ← NEW — owns Hybrid/Forward/Forward+ choice
│   │   ├── GBufferPass.h/.cpp    ← was GBufferPassSystem
│   │   ├── DeferredLightingPass.h/.cpp ← was DeferredLightingPassSystem
│   │   ├── ForwardOpaquePass.h/.cpp ← was ForwardOpaquePassSystem
│   │   ├── ForwardPlusPass.h/.cpp← was ForwardPlusOpaqueGeometry + prep
│   │   ├── TransparencyPass.h/.cpp ← was TransparencyPassSystem
│   │   ├── SkyboxPass.h/.cpp     ← was SkyboxPassSystem
│   │   └── GBuffer.h/.cpp        ← moved from renderer/
│   │
│   ├── shadows/
│   │   └── ShadowFeature.h/.cpp  ← NEW — shadow pass as a schedulable feature
│   │
│   ├── atmosphere/
│   │   ├── AtmosphereFeature.h/.cpp ← NEW
│   │   ├── PhysicalSky.h/.cpp    ← NEW — Bruneton/Hillaire model
│   │   ├── VolumetricClouds.h/.cpp ← NEW
│   │   └── VolumetricFog.h/.cpp  ← NEW
│   │
│   ├── raytracing/
│   │   ├── RayTracingFeature.h/.cpp ← NEW — owns BLAS/TLAS management
│   │   ├── AccelerationStructure.h/.cpp ← NEW
│   │   ├── RTAO.h/.cpp           ← NEW
│   │   ├── ReSTIRDI.h/.cpp       ← NEW — direct illumination resampling
│   │   └── HybridRayTracing.h/.cpp ← NEW — raster primary + RT secondary
│   │
│   ├── pathtracing/
│   │   ├── PathTracingFeature.h/.cpp ← NEW
│   │   ├── PathTracer.h/.cpp     ← NEW
│   │   └── PathTracerAccumulator.h/.cpp ← NEW
│   │
│   ├── neural/
│   │   ├── NeuralRenderFeature.h/.cpp ← NEW — NeRF / neural radiance
│   │   ├── NeuralRadianceCache.h/.cpp ← NEW
│   │   └── InstantNGP.h/.cpp     ← NEW — hash grid encoding
│   │
│   └── editor/
│       ├── EditorFeature.h/.cpp  ← NEW — wraps all editor-only passes
│       ├── EntityIDPass.h/.cpp   ← was entity ID inline in RenderPipeline
│       ├── EditorOverlayPass.h/.cpp ← was EditorOverlayPassSystem
│       └── DebugDrawPass.h/.cpp  ← was DebugDrawRenderer
│
│   ── TEMPORAL (accumulation, upscaling, denoising) ──────────────────
├── temporal/
│   ├── TemporalSystem.h/.cpp     ← NEW — owns history buffers, jitter, TAA
│   ├── MotionVectorPass.h/.cpp   ← NEW — per-object + per-bone velocity
│   ├── HistoryBuffer.h/.cpp      ← NEW — previous frame resource management
│   ├── TAA.h/.cpp                ← NEW
│   ├── upscaling/
│   │   ├── IUpscaler.h           ← NEW — DLSS/FSR/XeSS behind one interface
│   │   ├── DLSSUpscaler.h/.cpp   ← NEW
│   │   ├── FSRUpscaler.h/.cpp    ← NEW
│   │   └── XeSSUpscaler.h/.cpp   ← NEW
│   └── denoising/
│       ├── IDenoiser.h           ← NEW — interface
│       ├── SVGFDenoiser.h/.cpp   ← NEW
│       ├── NRDDenoiser.h/.cpp    ← NEW — ReLAX / ReBLUR (Nvidia NRD)
│       └── DLSSRayReconstruction.h/.cpp ← NEW
│
│   ── POST PROCESSING ─────────────────────────────────────────────────
├── post/
│   ├── PostProcessor.h/.cpp      ← KEEP, clean up
│   ├── PostStack.h/.cpp          ← NEW — ordered effect chain
│   ├── ToneMapping.h/.cpp        ← NEW — split out of PostProcessor
│   ├── Bloom.h/.cpp              ← NEW — split out
│   ├── DepthOfField.h/.cpp       ← NEW
│   ├── MotionBlur.h/.cpp         ← NEW
│   ├── FXAA.h/.cpp               ← NEW — split out
│   ├── SMAA.h/.cpp               ← NEW
│   ├── ColorGrading.h/.cpp       ← NEW
│   ├── AutoExposure.h/.cpp       ← NEW
│   └── LensEffects.h/.cpp        ← NEW — flares, aberration, vignette
│
│   ── RESOURCES (GPU-side assets) ────────────────────────────────────
├── resources/
│   ├── Mesh.h/.cpp               ← KEEP
│   ├── Shader.h/.cpp             ← KEEP
│   ├── Texture2D.h/.cpp          ← KEEP
│   ├── Texture3D.h/.cpp          ← KEEP
│   ├── TextureCube.h/.cpp        ← KEEP
│   ├── TextureArray.h/.cpp       ← KEEP
│   ├── TextureTypes.h            ← KEEP
│   ├── TextureLoader.h/.cpp      ← KEEP
│   ├── TextureDescriptor.h       ← moved from root
│   ├── TextureStreamer.h/.cpp     ← NEW — virtual texture page streaming
│   ├── PassResourceBuilders.h/.cpp ← KEEP
│   └── RenderResourcePool.h/.cpp ← NEW — pooled transient resource alloc
│
│   ── 2D / SPRITES / TEXT (unchanged, just organized) ─────────────────
├── 2d/
│   ├── Sprite.h/.cpp
│   ├── SpriteRenderer.h/.cpp
│   ├── Font.h/.cpp
│   ├── TextRenderer.h/.cpp
│   └── Primitives.h/.cpp
│
│   ── DEBUG & TOOLING ─────────────────────────────────────────────────
└── debug/
    ├── DebugDrawRenderer.h/.cpp  ← moved from root
    ├── RenderDocIntegration.h/.cpp ← NEW — RenderDoc capture API
    ├── GpuProfiler.h/.cpp        ← or keep in core/
    └── RenderStats.h/.cpp        ← NEW — frame stats, pass timings
```

---

**The rules this layout enforces:**

`scene/` → knows nothing about GPU. Pure data contract.

`core/` → knows RHI, owns frame-level resources. Knows nothing about features.

`framegraph/` → pure graph machinery. No feature logic, no pass content.

`scheduler/` → knows features and the frame graph. Orchestrates, never renders.

`visibility/` → only culling and draw list production. No lighting, no materials.

`geometry/` → only geometry representation and submission. No lighting knowledge.

`lighting/` → knows geometry (consumes draw lists), knows RHI. Never touches post.

`features/` → each subfolder is a self-contained rendering technique. Depends on visibility, geometry, lighting, materials, shaders. Registers passes into the scheduler.

`temporal/` → consumes feature outputs (noisy buffers), produces clean/upscaled result. No scene knowledge.

`post/` → consumes HDR from temporal. Pure image-space. No scene knowledge.

`resources/` → GPU asset lifetime only. No pass logic.

---

**What moved where from your current structure:**


| Old location                                     | New location                             | Reason                             |
| ------------------------------------------------ | ---------------------------------------- | ---------------------------------- |
| Root scatter (`ShaderKey`, `ShaderLoader`, etc.) | `shaders/`                               | Grouped by domain                  |
| Root scatter (`MeshDescriptor`, `RenderContext`) | `scene/` or `core/`                      | Proper ownership                   |
| `renderer/RenderPipeline`                        | `core/RenderWorld`                       | Broken up, not a god object        |
| `renderer/graph/`                                | `scheduler/` + `framegraph/`             | Separated orchestration from graph |
| `passes/` flat list                              | `features/raster/` + others              | Each feature owns its passes       |
| `shadows/`                                       | `lighting/shadows/`                      | Shadows are a lighting concern     |
| `ibl/`, `reflection/`                            | `lighting/ibl/`, `lighting/reflections/` | Same                               |
| `lighting/`                                      | `lighting/` + `lighting/gi/`             | GI strategies added cleanly        |
| `post/PostProcessor`                             | `post/` with split effects               | Each effect is independent         |


---

## 1. The Frame Graph is your constitution — respect it absolutely

Every pass that touches the GPU **must** go through the frame graph. No exceptions.

The moment you allow one system to "just record a command directly" outside the graph — because it seemed simpler at the time — you break barrier correctness, you lose aliasing, and you lose the ability to reason about pass order. This is how `PostProcessor` ended up orphaned after the graph in your current design.

**The rule:** If it writes to a texture or buffer, it's a graph pass. Period. Post processing, denoising, upscaling, editor overlays — all of it. The graph is not just for scene passes.

---

## 2. `IRenderFeature` is a contract, not a suggestion

Every feature must implement the same lifecycle:

```
OnRegister       → declare capabilities, resource needs
OnFramePrepare   → per-frame CPU-side updates (light buffers, transforms)
OnSchedulePasses → declare passes into the frame graph THIS frame
OnShutdown       → release owned GPU resources
```

The critical one is `OnSchedulePasses`. A feature must be able to say "I need nothing this frame" — path tracing disabled, neural rendering not active, etc. — and contribute zero passes. The scheduler must never know what a feature does internally. It only sees the declared passes.

**Never let features talk to each other directly.** If path tracing needs a light buffer that LumenGI produced, it reads it as a named frame graph resource from the blackboard. The blackboard is the only inter-feature communication channel.

---

## 3. The Blackboard is sacred shared state

`FrameGraphBlackboard` is the typed registry of "what this frame produced so far." Every resource that multiple features need — the HDR target, the depth buffer, the shadow atlas, the light grid, motion vectors — lives on the blackboard with a well-defined name and ownership rule.

**Ownership rule:** exactly one feature writes each blackboard resource. Any number can read it. If two features try to write the same resource, that's a design error you catch at registration time, not at runtime.

This replaces the ad-hoc `m_hdrRT`, `m_depthRT`, `m_shadowMap` member variables scattered across `RenderPipeline`. Those still exist physically in `RenderResources`, but they are accessed through the blackboard by name, not by pointer-chasing through a god object.

---

## 4. Dependency direction is a one-way street

Draw this on paper and enforce it:

```
scene → core → framegraph → scheduler → features → lighting/geometry/visibility → resources/shaders → rhi
```

**Nothing in a lower layer may include a header from a higher layer.** `rhi/` never includes anything from `features/`. `lighting/` never includes anything from `scheduler/`. `framegraph/` never includes a feature header.

The moment you violate this — because it was convenient, because you just needed one flag — you've created a cycle that will eventually force a recompile of everything when anything changes. In a large codebase this becomes catastrophic.

Enforce it with a build system check or at minimum a code review rule. Your CMakeLists target dependencies should mirror this diagram exactly.

---

## 5. `RenderCapabilities` gates everything at startup, not at runtime

Query the device once at startup. Populate `RenderCapabilities`:

```cpp
bool supportsRayTracing;
bool supportsHardwareMeshShaders;
bool supportsDLSS;
bool supportsVariableRateShading;
bool supportsBindless;
uint32_t maxMSAASamples;
// etc.
```

Every feature reads this at `OnRegister` time and either registers fully, registers a fallback, or does not register at all. After registration, no feature ever calls `vkGetPhysicalDeviceFeatures` again, and no feature has runtime `if (device supports RT)` branches in hot paths.

This also means your feature registry knows the full capability-constrained feature set before the first frame renders.

---

## 6. Geometry providers are pure data producers — they never know about lighting

`IGeometryProvider` takes a `DrawList` from visibility and emits GPU work — draw calls, compute dispatches, whatever the representation needs. It never reads the light grid. It never reads the shadow map. It never knows what lighting model is active.

This is the boundary that lets you add Gaussian splatting or NeRF without touching any lighting code, and lets you change your GI system without touching any geometry code.

The thing that connects geometry output to lighting input is the GBuffer (deferred) or the shader binding (forward) — and that binding lives in the pass, not in either system.

---

## 7. Temporal is a system, not an afterthought

Decide upfront: every feature that produces a noisy or temporally unstable buffer registers it with `TemporalSystem` at `OnRegister` time:

```cpp
temporalSystem.RegisterHistory("RTAO", format, extent);
temporalSystem.RegisterHistory("PathTrace.Radiance", format, extent);
```

`TemporalSystem` owns the history buffer allocation, the ping-pong swap, the jitter sequence, and the validity flags (did the camera cut? invalidate history). Features never allocate their own "previous frame" textures. They request them from `TemporalSystem` and get back a handle.

This is the only way denoising and upscaling can work correctly — they need to know about all temporal consumers at once to sequence correctly and handle history invalidation globally.

---

## 8. The upload phase must be real, not a stub

Your current `UpdateUploads()` is a no-op. In the new design this becomes the most important phase before rendering:

```
BeginFrame
  → UploadPhase (GPU scene, transforms, materials, light buffers — all of it)
  → FrameScheduler::Schedule (determine active features, declare passes)
  → FrameGraph::Compile (barriers, aliasing)
  → FrameGraph::Execute (GPU recording)
EndFrame
```

The upload phase must be completed before any pass executes. If you allow uploads to happen lazily inside pass callbacks, you get incorrect data, missing barriers around upload → read transitions, and non-deterministic frame behavior.

This is especially critical for Nanite (cluster streaming uploads), virtual shadow maps (page uploads), and neural rendering (weight buffer updates).

---

## 9. Never store a raw `ICommandList*` across a frame boundary

This is an RHI-level rule from your own `RENDER_MODULE.md` but it becomes even more critical with async compute. With multiple queues in play — graphics, async compute, copy — a command list belongs to exactly one queue, one frame slot, one lifetime. Features receive it as a parameter during `Execute`, they do not store it.

Related: every feature that wants to use async compute declares this in `OnSchedulePasses`. The scheduler assigns it to the compute queue. The feature never manually submits to a queue.

---

## 10. Shaders and permutations are centrally compiled, never on-demand in a pass

The pattern to avoid:

```cpp
// INSIDE a pass callback — BAD
auto pipeline = m_pipelineCache.GetOrCompile(key); // may stall
```

All permutations a feature needs must be declared at `OnRegister` time and compiled asynchronously before the first frame that feature is active. The pass callback receives an already-compiled pipeline. It never triggers a compile.

This means `ShaderPermutationKey` needs to be expressive enough to cover all variants at declaration time — RT on/off, MSAA level, material domain, etc.

---

## 11. Resource naming is a formal contract

Every named resource in the frame graph and blackboard needs a consistent naming convention established before you write features. A good pattern:

```
"GBuffer.Albedo"
"GBuffer.Normal"
"GBuffer.Depth"
"Shadow.Atlas"
"Shadow.CascadeMatrices"
"Lighting.HDR"
"Lighting.LightGrid"
"Temporal.MotionVectors"
"PostProcess.Output"
```

These are string constants in a central header — similar to your existing `FrameGraphPassNames.h` — but covering resources too. Anyone who hardcodes `"hdr_target"` in a string literal creates a bug that only shows at runtime.

---

## 12. The editor is a feature, not a special case

In your current design, entity ID picking, debug lines, and editor overlays are hardcoded steps in `RenderPipeline::Render`. They run unconditionally (guarded by flags) and they exist outside the feature system.

In the new design `EditorFeature` registers into the scheduler like any other feature. When shipping a game build, you compile it out entirely — not with `#ifdef EDITOR` scattered everywhere, but by simply not registering `EditorFeature`. The scheduler never sees it. Zero overhead.

---

## 13. Profile everything before you optimize anything

Add `GpuProfiler` scope markers to every pass from day one:

```cpp
GpuProfiler::Scope scope(cmd, "Shadow.Cascade0");
```

This gives you frame captures that show exactly where time goes. The most common mistake in building a new render module is optimizing things that aren't bottlenecks because you couldn't see the actual timings.

Use RenderDoc integration from the start. Build `RenderDocIntegration` early — the ability to programmatically capture a frame saves enormous debugging time when barriers are wrong or resources are uninitialized.

---

## 14. Write one feature completely before writing the second

The most dangerous time in building this system is early, when the interfaces aren't proven yet. If you write `RasterFeature`, `RayTracingFeature`, and `PathTracingFeature` simultaneously, you'll discover the `IRenderFeature` interface is wrong when all three are half-done and you have to refactor all of them.

The right order:

1. Get `RHI` + `FrameGraph` + `FrameScheduler` + `RenderWorld` compiling with no features
2. Implement `RasterFeature` completely — Forward path only, no Forward+, no deferred. One feature, working end to end
3. Prove the blackboard, the scheduler, the upload phase all work correctly
4. Add deferred to `RasterFeature` as a strategy — prove mode switching works
5. Add `ShadowFeature` — prove inter-feature resource sharing via blackboard works
6. Only now add a second major feature (`RayTracingFeature` or `TemporalSystem`)

Every new feature either confirms the interfaces are right or reveals a gap. Doing it sequentially means you fix gaps once, not six times simultaneously.

---

That's the guidance that actually matters. The directory structure gives you the map. These rules are what keep you from redrawing the map in six months.