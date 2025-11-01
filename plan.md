## OpenEye City Sandbox — End‑to‑End Plan (Pure OpenGL)

### Vision
- Build a multiplayer‑ready VR‑capable urban sandbox with arenas, exploration, media screens, and future AI control.
- Ship a playable game first; the library grows pragmatically as features are needed.

### Non‑negotiables
- Pure and only OpenGL rendering (no other graphics APIs).
- Must handle very large 3D scenes efficiently (city‑scale) at 60+ FPS.
- Full post‑processing pipeline (HDR, tone mapping, bloom, SSAO/SSR, AA).
- Render‑to‑texture (RTT) for video on in‑world quads; future headless browser (Chromium) to quad.
- Bullet physics integration; scripting via Lua; development tools with ImGui; final game without tools.
- If any method/implementation proves problematic, consult before switching approaches.

### Target Platform & Tech Stack
- OS: Windows 10+
- Language/Std: C++20
- Graphics: OpenGL 4.5 Core Profile (MRT, UBOs/SSBOs, instancing, sampler objects, DSA)
- Build: CMake
- Third‑party (present): GLFW, GLAD, GLM, stb, ImGui, Assimp, EnTT, nlohmann/json
- Third‑party (to add): Bullet, FFmpeg (or libav*), Chromium Embedded Framework (CEF) later

### Repository Layout
```text
OpenEye/
├─ src/
│  ├─ core/
│  │  ├─ window/                  # GLFW window, GL context, swapchain
│  │  ├─ input/                   # Keyboard/mouse (VR later)
│  │  ├─ time/                    # Frame timing
│  │  └─ log/                     # Logging, asserts
│  ├─ ecs/                        # EnTT setup, components, systems
│  ├─ rendering/
│  │  ├─ resources/               # Shader, Mesh, Texture, Material, UBO
│  │  ├─ optimization/            # FrustumCuller, Octree, LODManager, Instancing, Batching
│  │  ├─ passes/                  # Geometry, Lighting, Shadow, Transparency, PostProcess
│  │  ├─ post/                    # ToneMapping, Bloom, SSAO, SSR, FXAA/TAA
│  │  ├─ rtt/                     # RenderTarget, VideoTexture, BrowserTexture, QuadRenderer
│  │  └─ debug/                   # DebugRenderer, GPU/CPU Profiler
│  ├─ physics/                    # BulletWorld, Colliders, Rigidbodies, CharacterController
│  ├─ scripting/                  # LuaEngine, Bindings, ScriptSystem
│  ├─ editor/                     # ImGui panels, Gizmos, Scene exporter (JSON)
│  ├─ game/                       # Player, City, Arenas, GameState
│  └─ utils/                      # Math helpers, file IO
├─ assets/                        # models/, textures/, materials/, scripts/, scenes/
├─ thirdparty/                    # vendored libs (already present)
└─ CMakeLists.txt                 # toplevel build
```

## Rendering Architecture (High‑Level)

```text
[Scene Culling] → [Geometry Pass (G‑Buffer)] → [Lighting/Shadow] → [Transparency] → [Post‑FX] → [UI/VR] → Present
   └─ Frustum + (optional) Occlusion
   └─ LOD selection, Instancing, Batching

G‑Buffer MRT (suggested):
  A) RGBA8 sRGB: Albedo + Opacity
  B) RG16F: Normal.xy (octa encoded), Roughness/Metalness packed in A
  C) RGBA16F: Emissive/Extra
  D) D24S8: Depth/Stencil
```

Key capabilities
- Deferred base for opaque; forward for alpha/transparency.
- Shadow mapping (directional + spot/point as needed), cascaded for sun.
- Post‑FX chain with ping‑pong FBOs (HDR tone mapping, bloom, color grading, SSAO, SSR, AA).
- Render targets pool (MSAA and non‑MSAA) with resolve.
- RTT textures updated from FFmpeg (video) and CEF OSR (browser, later).

## End‑to‑End Implementation Plan (Sequential)

### Phase 0 — Bootstrap & Build System
- Create toplevel `CMakeLists.txt` wiring GLFW, GLAD, GLM, stb, ImGui, EnTT, Assimp, nlohmann/json.
- Set C++20, warnings as errors in dev, `/permissive-` and `/Zc:preprocessor` on MSVC.
- Create `src/core/window` for context init: OpenGL 4.5 core, debug output, sRGB framebuffer, VSync toggle.
- Minimal main loop: clear color, basic input, fixed timestep accumulator.

Deliverables
- Window opens, GL context 4.5 verified, debug output prints errors, ESC quits.

Acceptance
- 120 FPS with empty scene at 1080p; GL debug layer clean.

### Phase 1 — Resource & Shader System
- Shader manager (compile, link, reflection, uniform/UBO binding, includes, `#define` variants).
- Hot‑reload in editor; precompiled cache for release.
- Texture loader (stb): sRGB/linear detection, mipmaps, anisotropy, sampler objects, texture cache.
- Mesh loader (Assimp): positions/normals/tangents/UVs, index buffer, VAO setup; mesh cache.
- Material system: shader + textures + parameters, state hashes to minimize state changes.

Deliverables
- Render a lit test mesh with PBR material (IBL stub ok) in forward path.

Acceptance
- Material/mesh reuse uses caches; shader recompiles on file change in editor build.

### Phase 2 — ECS & Scene Basics
- EnTT registry; components: `Transform`, `Renderable`, `Camera`, `Light`, `Name`, `AABB`.
- Hierarchical transforms with dirty propagation; world matrix cache.
- Camera system with view/proj and extracted frustum planes.

Deliverables
- Spawn N objects (cubes) with transforms; basic camera fly mode.

Acceptance
- Transforms update O(k) for k dirty nodes; frustum planes correct via debug draw.

### Phase 3 — Culling, LOD, Instancing, Batching
- AABB generation per mesh; CPU frustum culling against planes.
- LODManager: distance thresholds, hysteresis per entity; up to 4 LODs/mesh.
- Instancing: per‑mesh/material instance buffer (mat4 or dual‑quat), divisor attributes.
- Static batching tool (offline): merge static meshes by material into big VBOs.

Deliverables
- City proxy scene (thousands of props) renders via instancing; LOD switches without popping.

Acceptance
- Culling < 1 ms; instance draw calls dramatically fewer than entity count.

### Phase 4 — Deferred Geometry & Lighting Passes
- G‑Buffer MRT FBO; geometry pass for opaque only.
- Lighting pass (fullscreen) with clustered or tiled light list (start simple: N small lights + 1 directional).
- Shadow mapping: cascaded directional (2–4 cascades), PCF filtering; point/spot optional.
- Depth pre‑pass switch (tunable) for overdraw heavy scenes.

Deliverables
- PBR‑ish lighting in deferred path; debug viewers for G‑Buffer and cascades.

Acceptance
- Lighting pass stable < 2 ms for moderate light counts at 1080p on target GPU.

### Phase 5 — Transparency & Special Materials
- Forward pass for transparent materials; sorted back‑to‑front.
- Optional OIT variant (weighted blended) behind a flag.
- Skybox/atmosphere pass; emissive and unlit materials handled.

Deliverables
- Glass windows, decals/emissives, skybox.

Acceptance
- Sorting artifacts rare; OIT mode compiles/runs when enabled.

### Phase 6 — Post‑Processing Pipeline
- Post chain manager with ping‑pong FBOs and configurable stack.
- Effects: HDR tone mapping (ACES), auto‑exposure (optional), bloom (dual‑filter), color grading (3D LUT), SSAO, SSR, FXAA; TAA optional.
- Quality presets and performance budget per pass.

Deliverables
- Toggleable effects via config/UI; screenshots match expected look.

Acceptance
- Total post budget ≤ 5 ms at 1080p on target GPU (with SSAO+Bloom+ToneMap+FXAA).

### Phase 7 — Render Targets & Render‑to‑Texture (RTT)
- `RenderTarget` wrapper: MSAA, resolve, depth, color formats; pooling.
- `QuadRenderer` for textured quads in 3D (with correct mip & anisotropy).
- `VideoTexture` (FFmpeg): decode thread → frame queue → YUV→RGB (sws_scale initially) → double‑buffered GL upload.
- Frame clock sync (video fps vs vsync), loop modes, pause, seek; multiple video screens.

Deliverables
- At least 3 independent video quads playing different files in‑scene.

Acceptance
- No frame drops for 1080p H.264 at 60 FPS for single screen; ≤ 2% main thread overhead per additional screen.

### Phase 8 — Physics (Bullet)
- Bullet world init; components: `RigidBody`, `Collider` (box/sphere/capsule/mesh), `CharacterController`.
- Transform sync (physics→render), sub‑stepping; simple raycasts.
- Debug drawing with `DebugRenderer`.

Deliverables
- Player capsule controller walking, colliding with static meshes.

Acceptance
- Stable stepping at 60 Hz; tunneling absent at normal speeds.

### Phase 9 — Editor (Development Environment)
- ImGui docking; panels: Scene Hierarchy, Inspector (Transform/Material/Physics/Script), Asset Browser, Profiler.
- ImGuizmo gizmos; picking (ray vs AABB/triangles); scene save/load (JSON via nlohmann/json).
- Shader hot‑reload; scene exporter for final game (data‑only build).

Deliverables
- Author a small city block by hand; save/load round‑trip.

Acceptance
- 1:1 scene fidelity after reload; edit latency minimal; no UI in release build.

### Phase 10 — Scripting (Lua)
- Lua 5.4 engine; `ScriptComponent` attach; `ScriptSystem` calls `init/update`.
- Bindings: Transform, Input, Spawns, Simple Physics queries; hot‑reload scripts.
- Script editor panel (basic) in editor.

Deliverables
- NPC patrol and a basic weapon scripted; hot‑reload changes live.

Acceptance
- No crashes on script reload; per‑frame script budget ≤ 1 ms in demo scene.

### Phase 11 — City Generation & Streaming (Foundations)
- Block/tile system; deterministic placement of buildings/props; seedable.
- Spatial tiles with async loading hooks (asset streaming stub).
- LOD & culling integrated in streaming boundaries.

Deliverables
- 1–2 km² proxy city streaming in/out while walking.

Acceptance
- Frame spikes < 2 ms during tile transitions; memory stable.

### Phase 12 — Arenas & Game Flow
- Teleport points from city → arena scenes → back; arena state machine (scores, timers, spawns).
- Weapons, targets, simple AI using physics raycasts.

Deliverables
- One complete arena (office/mall) playable loop end‑to‑end.

Acceptance
- Enter/exit flow seamless; scores tracked; stable 60 FPS.

### Phase 13 — VR (Prep, then Integration)
- Prep: stereo render abstraction, MSAA, fixed‑foveated (config), late‑latch hook.
- Integration (later): OpenXR session, VR camera/controllers, VR UI.

Deliverables
- Non‑VR build remains primary; VR path guarded behind build flag.

Acceptance
- Stereo render performs within targets on supported hardware.

### Phase 14 — Browser to Quad (Future)
- CEF OSR integration; pixel buffer → GL texture upload (BGRA8); input routing for focus quads.
- Security sandbox & process isolation.

Deliverables
- Interactive webpage on a quad; simple UI navigation.

Acceptance
- 30–60 FPS at 720p OSR; main thread overhead capped.

### Phase 15 — Multiplayer (Foundation)
- Choose transport; entity replication model; snapshot interpolation.
- Minimal sync of transforms and gameplay events within arena.

Deliverables
- Two local clients see each other and interact in arena.

Acceptance
- Jitter handled; authority model defined; basic anti‑cheat hooks.

## Detailed Designs (Reference)

### Culling & LOD
- AABB per entity; plane tests; temporal coherence cache.
- LOD thresholds with hysteresis; screen‑space error optional.
- Debug overlays (colors per LOD; culled count).

### Instancing & Batching
- Instance buffer per (mesh, material); mat4 with divisor=1; frustum cull per instance on CPU.
- Static batching tool merges meshes by material; stores transform baked.

### Shadows
- Cascaded shadow maps for directional light; 2–4 cascades; stable fit; PCF/PCSS optional.

### Post‑FX
- Ping‑pong FBO manager; pass graph configurable; HDR pipeline throughout.
- Tone mapping (ACES), Bloom (threshold → downsample → blur → upsample), SSAO (spiral kernel), SSR (roughness‑aware), AA (FXAA first, TAA optional).

### RTT & Video
- RenderTarget pool (sizes, formats) with MSAA resolve.
- FFmpeg decode thread → queue → sws_scale YUV→RGB → glTextureSubImage2D; double‑buffered textures; fence sync.
- Future CEF OSR writes BGRA8 buffer → GL; cursor/input forwarding to world quad.

### Physics
- Bullet world step (fixed 60 Hz); `RigidBody`, `Collider`, `CharacterController`; kinematic→dynamic sync.
- Raycasts for hitscan; triggers for zones; debug draw.

### Editor
- Panels: Hierarchy, Inspector, Assets, Profiler; ImGuizmo; pick/raycast; JSON save/load; shader hot‑reload.

### Scripting
- Lua 5.4; `init/update` convention; bindings for Transform/Input/Physics; hot‑reload.

## Performance Targets & Budgets
- 1080p60 on target GPU with city block:
-   Culling + LOD + instancing: ≤ 1.5 ms
-   Geometry + lighting (opaque): ≤ 5.0 ms
-   Transparency: ≤ 1.5 ms
-   Post‑FX common set: ≤ 5.0 ms
-   Video screen (1080p H.264): ≤ 0.5 ms upload/frame

## Build & Run
- CMake presets for Debug/Dev (editor, hot‑reload) and Release (no editor).
- Windows: ensure paths for Assimp DLL, later Bullet/FFmpeg/CEF.
- Start target: `Editor` (dev) and `Game` (final runtime loader).

## Coding & Data Conventions
- Coordinate system: right‑handed, +Y up, meters as unit, Z forward.
- File naming: `PascalCase` types, `camelCase` functions/vars; `UPPER_SNAKE` for constants.
- Shaders: `shaders/pbr/`, `shaders/post/`; shared includes in `shaders/include/`.

## Risks & Mitigations
- FFmpeg/CEF integration complexity → isolate behind `rtt/` with clean interfaces; fallback stubs.
- Performance variance → build profiler overlays and frame capture tooling early.
- Asset memory pressure → streaming and LOD mandatory; texture compression pipeline later.

## Milestone Checklist (Go/No‑Go)
- M0: Window + GL debug OK
- M1: Resource systems + PBR test OK
- M2: ECS + transforms OK
- M3: Culling/LOD/Instancing OK (10k+ objects)
- M4: Deferred + Shadows OK
- M5: Post‑FX baseline OK
- M6: Video on quads (≥3) OK
- M7: Physics + controller OK
- M8: Editor save/load OK
- M9: Scripting OK
- M10: City streaming demo OK
- M11: Arena loop OK

---
This plan is intentionally sequential, with measurable deliverables and acceptance criteria at each phase. If any chosen method proves impractical, we will pause and review alternatives together before changing approach.


