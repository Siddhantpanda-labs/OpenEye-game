# Runtime Loader Check & Capabilities

## 1. Proper check: is the runtime loader fine?

### Pipeline (verified)

1. **Entry** — `main.cpp` → `RunPlayer(argc, argv)`.
2. **Config** — `FindConfigPath` / `ParseArgs` → `RuntimeOptions` (scene path, asset roots, window, time, profiles). Defaults: `scenes/main.json`, `assets/`.
3. **Engine init** — `Engine::Initialize`, logging, headless support.
4. **Scene load** — `World::LoadScene(options.scenePath)` loads JSON into `WorldState` (entities, registry, graph, components). Fails with exit code 6 if file missing or invalid.
5. **Asset binding** — `BindAssets(world, options.assetRoots, assetRegistry)` collects refs from the world (`CollectAssetReferences`), resolves each ID against asset roots, loads Mesh/Material/Animation into the registry. Strict mode fails on missing/unknown (exit 7).
6. **Window** — Optional; created from `options` if not headless (exit 8 on failure).
7. **Bridges** — `WorldRenderBridge`, `WorldPhysicsBridge`, `WorldAnimationBridge`, `WorldScriptingBridge` initialized and wired. Render init can fail (exit 9), physics (exit 10).
8. **Loop** — `Engine::Run(update, render)`: each frame: poll events, physics step, animation update, scripting update, world update; then build render scene from world and render; optional overlay and stats.

### Fix applied

- **Mesh/material path resolution** — The render bridge previously used scene asset IDs (e.g. `models/cube.obj`, `materials/solid_red.mat`) directly. The registry and model loader expect paths that are either absolute or resolved against asset roots. **Fix:** In `WorldRender.cpp`, `BuildScene` now resolves mesh and material IDs with `ResolveAssetPath(id, options.assetRoots)` before calling `LoadModelEntry(...)` and `assetRegistry->GetMaterial(...)`, so the same asset roots used by `BindAssets` are used when building the render scene. This keeps the runtime loader correct for typical `--scene` + `--assets` usage.

### Result

- **Runtime loader is in good shape** for the intended use: one executable, config/scene path and asset roots, load scene → bind assets → run loop. Scene and asset paths are resolved consistently; missing scene or strict-asset failure is reported with clear exit codes.

---

## 2. What the runtime loader can do

- **Run any scene** — As long as you provide a valid scene JSON (see world serialization format) and the right asset roots, the same executable runs different “games” or levels by changing `--scene` and `--assets`.
- **Config and profiles** — JSON config and CLI overrides; `dev` vs `shipped` profiles (overlay, stats, strict assets, fixed timestep).
- **Time control** — Realtime, fixed timestep, or replay; pause, step, time scale.
- **Headless** — Run without a window (simulation only).
- **Scripting** — Lua scripts attached via world `Script` component are loaded and updated; they get the world API (entities, transforms, cameras, lights, etc.) and can drive behavior.
- **Physics** — Rigid bodies and colliders in the scene are bound to the physics world; transform sync and velocity read/write are supported.
- **Animation** — Animator component + clip/skeleton assets; bone matrices for skinned meshes.
- **Overlay and stats** — Optional on-screen overlay and per-second FPS/entity stats.

---

## 3. What the runtime loader can render

Rendering is driven by the **world** components that the render bridge reads each frame. It builds an `api::RenderScene` and passes it to the **deferred** pipeline.

### Drawn objects

- **Renderable** — Uses `meshAssetId` (and optional LOD list) and `materialAssetId`. Mesh is loaded via `ModelLoader::LoadModel(resolvedPath)` (supports e.g. `.obj`); material from registry by resolved path. If no material is set or resolved, the first material from the loaded model is used, or the bridge’s default material (forward basic shader).
- **MeshRenderer** — Same idea: `meshAssetId` and `materialAssetId` resolved against asset roots, then same load/resolve as above. Both Renderable and MeshRenderer end up as `Drawable`s in `scene.opaque`.

So the runtime can render:

- **Static meshes** — Any mesh format supported by the engine’s model loader (e.g. OBJ, FBX via Assimp), with a single material per drawable (first material from model or override from scene).
- **Skinned meshes** — If an entity has both a mesh and animator data, the bridge can supply bone matrices and use the skinned material path.
- **One primary camera** — From world `Camera` components (primary wins); perspective or ortho, FOV/aspect/near/far from component.
- **Lights** — One directional (from first Directional light entity), plus any number of point and spot lights from world `Light` components (position/direction from Transform).

### Asset types that affect rendering

- **Mesh** — Referenced by `meshAssetId` (e.g. `models/cube.obj`). Loaded as a model; first mesh (and first material) used for the drawable.
- **Material** — Referenced by `materialAssetId` (e.g. `materials/solid_red.mat`). Loaded from `.mat` JSON (shader path, albedo, metallic, roughness, textures, etc.). Must be resolvable under `options.assetRoots` and loaded into the registry (via `BindAssets`) so the render bridge can find it by resolved path.
- **Shader** — Materials reference shaders (e.g. `../shaders/forward` or `assets/shaders/passes/forward/forward`). The default/fallback material uses hardcoded paths under `assets/shaders/passes/forward/` (e.g. forward, forward_skinned).

### What it does *not* render (out of scope for this loader)

- No 2D sprites, particles, or fluids in this document’s “what it can render” list (those may exist elsewhere in the engine).
- No custom post-process stacks beyond what the deferred pipeline already does.
- No VR/XR; single main view.

---

## 4. Summary

- **Check:** The runtime loader pipeline is correct; scene and asset roots are used consistently, and mesh/material paths are now resolved in the render bridge so that the same asset roots used at load time are used at render time.
- **Capabilities:** Run any valid scene with config/profiles, time control, scripting, physics, animation, overlay/stats, and optional headless mode.
- **Rendering:** Opaque meshes (static or skinned) from world Renderable/MeshRenderer + mesh/material assets, one primary camera, one directional light plus multiple point/spot lights. Mesh and material paths are resolved against asset roots so that e.g. `scenes/main.json` plus `assets/` correctly shows a colored cube or other content that references `models/...` and `materials/...`.
