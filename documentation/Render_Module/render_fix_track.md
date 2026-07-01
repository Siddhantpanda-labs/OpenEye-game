Yes. Best way is to do this as a staged refactor with hard checkpoints, not a big-bang rewrite.

## Implementation Order (production-grade)

1. **Frame Coordinator (editor-wide, single source of truth)**
- Add `EditorRenderFrameCoordinator` that runs once per editor frame.
- Responsibilities:
  - One `FlushTransforms + CaptureRenderSnapshot` per frame.
  - One global dirty-set update pass.
  - One shared render cache update.
  - Per-panel camera render requests only.
- Remove snapshot/build calls from individual panels.

2. **Persistent Scene Cache (entity + asset keyed)**
- Add cache entries keyed by `(EntityId, MeshAssetId, MaterialAssetId, Submesh, LayerFlags)`.
- Store resolved GPU handles, bounds, transform index, light payload.
- Keep stable IDs/generation to detect entity recreation safely.
- Panels read immutable frame cache views.

3. **Dirty-Tracking Pipeline**
- Introduce dirty flags:
  - `TransformDirty`, `RenderableDirty`, `MaterialDirty`, `MeshDirty`, `LightDirty`, `VisibilityDirty`.
- Feed from world command/mutation layer (or diff previous snapshot if event feed unavailable).
- Update only dirty cache entries; avoid full rebuild.

4. **SoA Frame Storage + Memory Reuse**
- Replace per-frame clear/reallocate vectors/maps with persistent SoA buffers:
  - transforms, draw instances, material handles, mesh handles, bounds, light arrays.
- Use `reserve` once for high-water marks; reuse capacity every frame.
- Avoid per-frame `unordered_map` rebuild for transform lookup in hot path (use packed indices table).

5. **Visibility System Split**
- Build two-stage visibility:
  - Stage A: global broadphase structure (BVH/grid) updated incrementally from dirty bounds.
  - Stage B: per-camera frustum/occlusion query against broadphase.
- Output per-camera visible instance lists only; do not rebuild global scene lists.

6. **Panel Render Requests (camera-dependent only)**
- Scene/Game/Preview panels submit camera + viewport to coordinator.
- Coordinator returns cached visible list handles.
- No panel performs world traversal, asset ID resolution, or scene rebuild.

7. **Resource Resolution Hardening**
- Move asset-ID → GPU-handle resolution to cache update stage only.
- Resolve-once + invalidate-on-change policy.
- Add fallback/material miss handling without branching in inner render loop.

8. **Threading Model Upgrade**
- Keep game/editor logic on main thread.
- Move render preparation + submission to render thread with double-buffered frame data.
- Main thread publishes immutable frame packet; render thread consumes previous packet.

9. **Instrumentation + Guardrails**
- Add timings for:
  - snapshot, dirty update, cache update, visibility, render submit (per panel).
- Add invariants:
  - exactly one snapshot/frame,
  - zero panel-local scene rebuilds,
  - no per-frame asset-ID lookups in render hot path.
- Add regression tests with Sponza-scale scenes.

---

## Suggested concrete rollout in your codebase

1. Land `EditorRenderFrameCoordinator` + move SceneView to coordinator path first.  
2. Add persistent draw/light cache + dirty update (still single-thread).  
3. Add visibility broadphase + per-camera lists.  
4. Migrate GameView/MaterialPreview to same coordinator.  
5. Then add render-thread buffering.

---

If you want, I can now start implementing **Phase 1** directly in your repo (coordinator + single-snapshot-per-frame + SceneView migration), then we iterate phase by phase.

---

## Done Checklist (Editor Performance Architecture)

- [x] Step 1 - Frame Coordinator (single snapshot source-of-truth)
  - Added `EditorRenderFrameCoordinator` and wired `BeginFrame(ctx)` once per editor frame in both editor runtime branches.
  - Added shared frame snapshot storage in `EditorContext` (`frameSnapshot`, `frameSnapshotValid`, `frameSnapshotSerial`).
  - Scene/Game panels now read the shared frame snapshot instead of issuing their own per-panel `FlushTransforms + CaptureRenderSnapshot` calls.
  - Files:
    - `EngineSource/app/editor/EditorRenderFrameCoordinator.{h,cpp}`
    - `EngineSource/app/editor/EditorContext.h`
    - `EngineSource/app/editor/EditorApp.cpp`
    - `EngineSource/app/editor/panels/SceneViewPanel.cpp`
    - `EngineSource/app/editor/panels/GameViewPanel.cpp`
    - `EngineSource/app/editor/CMakeLists.txt`

- [x] Step 2 - Persistent Scene Cache (entity + asset keyed)
  - Added `EditorPersistentSceneCache` keyed by `(EntityId, meshAssetId, materialAssetId, submeshIndex, layerFlags)`.
  - Cache stores resolved mesh/material handles and reuses them across frames for the new scene-builder path.
  - Wired cache lifecycle to frame coordinator cadence:
    - `BeginFrame(frameSerial)` called from `EditorRenderFrameCoordinator`.
    - `EndFrame()` called once after editor panel pass each frame (prunes untouched keys).
  - Integrated cache into `EditorSceneBuilderNew` so Scene/Game new-pipeline builds resolve via cache first.
  - Files:
    - `EngineSource/app/editor/EditorPersistentSceneCache.{h,cpp}`
    - `EngineSource/app/editor/EditorSceneBuilderNew.{h,cpp}`
    - `EngineSource/app/editor/EditorContext.h`
    - `EngineSource/app/editor/EditorRenderFrameCoordinator.cpp`
    - `EngineSource/app/editor/EditorApp.cpp`
    - `EngineSource/app/editor/panels/SceneViewPanel.cpp`
    - `EngineSource/app/editor/panels/GameViewPanel.cpp`
    - `EngineSource/app/editor/CMakeLists.txt`
- [x] Step 3 - Dirty-Tracking Pipeline
  - Added `EditorSceneDirtyTracker` with explicit flags:
    - `TransformDirty`, `RenderableDirty`, `MeshDirty`, `MaterialDirty`, `LightDirty`, `VisibilityDirty`.
  - Dirty tracking source is frame-to-frame snapshot diff (current vs previous signatures for transforms/renderables/mesh-renderers/lights), including removals.
  - Wired into frame coordination:
    - `BeginFrame(snapshot)` runs once per frame after shared snapshot capture.
    - Cache-affecting dirty entities are emitted and used to invalidate per-entity entries in `EditorPersistentSceneCache`.
  - Files:
    - `EngineSource/app/editor/EditorSceneDirtyTracker.{h,cpp}`
    - `EngineSource/app/editor/EditorRenderFrameCoordinator.cpp`
    - `EngineSource/app/editor/EditorPersistentSceneCache.{h,cpp}`
    - `EngineSource/app/editor/EditorContext.h`
    - `EngineSource/app/editor/EditorApp.cpp`
    - `EngineSource/app/editor/CMakeLists.txt`
- [x] Step 4 - SoA Frame Storage + Memory Reuse
  - `EditorSceneBuilderNew` now uses persistent SoA transform lookup slots keyed by entity index/generation (`m_transformSlotGeneration`, `m_transformSlotIndex`) rebuilt from snapshot transform arrays each frame.
  - Replaced hot-path `snapshot.GetTransform(...)` map lookups during drawable/light conversion with O(1) slot lookup against the SoA tables.
  - Added proactive `reserve(...)` high-water reuse for draw/light/output entity arrays to avoid frame-local realloc churn after clear.
  - Files:
    - `EngineSource/app/editor/EditorSceneBuilderNew.{h,cpp}`
- [x] Step 5 - Visibility System Split
  - Added `EditorVisibilitySystem` with two-stage flow:
    - Stage A (`BeginFrame`): build frame-global visibility candidates from snapshot renderables/mesh-renderers + transform positions (broadphase set).
    - Stage B (`BuildCameraVisibility`): per-camera frustum refine to a visible-entity set (sphere-vs-frustum).
  - Integrated into editor frame architecture:
    - `EditorRenderFrameCoordinator` now runs visibility Stage A once per frame.
    - `EditorSceneBuilderNew` runs Stage B per camera and filters draw emission through `IsVisible(entity)`.
  - Wired through context + app ownership so Scene/Game new-pipeline builds both consume the same visibility subsystem.
  - Files:
    - `EngineSource/app/editor/EditorVisibilitySystem.{h,cpp}`
    - `EngineSource/app/editor/EditorRenderFrameCoordinator.cpp`
    - `EngineSource/app/editor/EditorSceneBuilderNew.{h,cpp}`
    - `EngineSource/app/editor/EditorContext.h`
    - `EngineSource/app/editor/EditorApp.cpp`
    - `EngineSource/app/editor/panels/SceneViewPanel.cpp`
    - `EngineSource/app/editor/panels/GameViewPanel.cpp`
    - `EngineSource/app/editor/CMakeLists.txt`
- [ ] Step 6 - Panel Render Requests API (camera-dependent only)
- [ ] Step 7 - Resource Resolution Hardening
- [ ] Step 8 - Threading Model Upgrade
    Main/UI thread
    ImGui, editor input, gizmos, world edits, command submission.
    Produces frame requests, does not do heavy render prep.
    Game/Simulation thread (can be same as main in edit mode initially)
    ECS update, animation, physics, transform solve.
    Publishes immutable snapshot to render side.
    Render thread
    Consumes previous snapshot.
    Dirty processing, cache updates, visibility, draw list build, command recording/submission/present.
    Then add worker jobs (thread pool, typically 4–12 workers depending CPU):

    Parallel culling jobs (by spatial tiles/chunks).
    Parallel scene/cache update jobs.
    Optional async asset decode/upload staging.
- [ ] Step 9 - Instrumentation + Guardrails
