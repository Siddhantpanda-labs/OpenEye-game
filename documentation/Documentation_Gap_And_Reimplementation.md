# Documentation vs Code — Gap and Reimplementation

After a revert (pull), documentation described features as "Done" that are no longer in the codebase. This file lists **what the docs say is implemented** vs **what is actually present**, and tracks reimplementation.

**References:** `Partials_Fix_Plan.md`, `Runtime_Efficiency_Unreal_Level_Plan.md`, `Runtime_Loader_Check.md`, `Serialization_Authority.md`.

---

## Partials_Fix_Plan — Gaps (reimplemented)

| # | Doc says | Status |
|---|----------|--------|
| 1 | **Mutation:** LoadSceneCommand, ApplyCommandSync; World::LoadScene via command; validator for LoadScene | **Done:** LoadSceneCommand, ApplyCommandSync, validator; api::World::LoadScene uses command path. |
| 2 | **Observation:** WorldObservation::GetView(); Observation has GetTransforms, GetRenderables, GetCameras, GetLights, GetMeshRenderers, GetRigidBodies, GetColliders; World::GetObservation(); player and Lua use GetObservation(); World batch getters deprecated | **Done:** WorldObservation::GetView() and batch getters; api::Observation batch getters; World::GetObservation(); player/Lua use GetObservation(); World batch getters deprecated. |
| 3 | **Audio:** WorldAudioBridge, PlayerApp creates bridge, Update in loop, listener from primary camera | **Done:** WorldAudioBridge; PlayerApp wires when !headless; Update syncs listener from primary camera. |
| 4 | **Assets:** Async + hot reload | Skipped per user request. | “done”) |
| 5 | **Serialization:** Serialization_Authority.md; SceneManager and BinarySerializer(Scene) deprecated in headers | **Done:** Deprecation comments in SceneManager.h and BinarySerializer.h (Serialization_Authority.md). |

---

## Runtime_Efficiency_Unreal_Level_Plan — Gaps

| Phase | Doc says | In code? | Action |
|-------|----------|----------|--------|
| 1 | GetXxxInto/GetXxxView on Observation; WorldRender/WorldPhysics/WorldAudio use reused buffers; Lua thread_local + GetXxxInto; no vector-returning getters | No GetXxxInto/GetXxxView; no reused buffers; map-based ComponentStore | Defer: do after Partials (Observation batch getters come first; then add Into/View and buffer reuse) |
| 2 | SoA Transform (ComponentStoreTransform), Dense stores (ComponentStoreDense) for Camera, Light, etc. | Only ComponentStore (unordered_map) | Defer: larger refactor |
| 3 | Job-based frame (StepPhysicsOnly, SyncTransformsToWorld, SubmitBatch) | Single-threaded | Defer |
| 4 | FrameAllocator, RenderScene with frame allocator, m_sceneFrameAllocator in WorldRenderBridge | None | Defer |
| 3b, 5, 6 | Render thread, SIMD, culling/batching/LOD | Not done | Defer |

---

## Runtime_Loader_Check — Verified in Code

- **ResolveAssetPath** in WorldRender.cpp BuildScene and AssetBinding: **present** (mesh/material resolved against asset roots).
- Pipeline (config → load scene → bind assets → bridges → loop): **present**.

---

## Reimplementation Order

1. **Serialization deprecation** — Add deprecation to api SceneManager and BinarySerializer (scene load/save).
2. **Observation layer** — Batch getters on Observation/WorldObservation, World::GetObservation(), player and Lua use Observation, deprecate World batch getters.
3. **Mutation (LoadScene via command)** — LoadSceneCommand, ApplyCommandSync (or equivalent), CommandValidator for LoadScene, World::LoadScene uses command path.
4. **Audio** — WorldAudioBridge, init/Update/shutdown in PlayerApp, listener from primary camera.

Later (optional): Phase 1 efficiency (GetXxxInto, buffer reuse), then Phase 2+ as needed.
