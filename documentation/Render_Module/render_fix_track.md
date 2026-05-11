# Render module fix track (`render_fix_track.md`)

**Purpose:** record live investigations + fixes for render-module crashes / regressions discovered during the editor + player migration. Each section is a single concrete problem with: log evidence, root cause, the fix, and the verification.
Short, precise log of render-module fixes during editor + player migration. One entry per problem: symptom, cause, fix, files.

Companion: `Render_steps_todo.md`, `editor_player_migration_track.md`, `new_render_module.md`.

## Build / verification commands

Repo root: `D:\Project\World3d\OpenEye`. Build directory: `build/` (single multi-config tree). Run from the repo root:

```
cd D:\Project\World3d\OpenEye
cmake --build build --target EyelibRendering         --config Debug
cmake --build build --target OpenEyeRenderingSandbox --config Debug
cmake --build build --target EyelibRenderingAPI      --config Debug
cmake --build build --target OpenEyeEditor           --config Debug
cmake --build build --target OpenEyePlayer           --config Debug
```

Run after a fix: editor binary at `Engine/bin/Debug/OpenEyeEditor.exe`, player at `Engine/bin/Debug/OpenEyePlayer.exe`. A fix is **not done** until everything that should still build, builds (per `editor_player_migration_track.md` Discipline §4).

---

## 2026-05-11 · Editor crash in `BlitMainHdrToDefault` (0xC0000005)

**Symptom:** crash mid-frame at `BlitMainHdrToDefault` execute; ImGui post-blit callback fired before `ImGui::Render()` (null draw data).

**Cause:** `Impl::BuildWorldAndFeatures` enabled the swapchain blit pass via `ownsDevice` heuristic. Editor's window-owning `sceneVN` therefore composed to swapchain inside its own graph, racing with the editor's own `ComposeSwapchain()` and firing ImGui draw before `ImGui::Render()`.

**Fix:** explicit public contract — `RenderPipeline::PresentationMode { Direct, Offscreen }` + `SetPresentationMode` / `GetPresentationMode`. Default `Direct` for `Initialize(window)`, `Offscreen` for `InitializeWithSharedDevice`. Editor flips `sceneVN` to `Offscreen`.

**Files:**
- `api/rendering/RenderPipeline.{h,cpp}` — add enum + setters; replace `ownsDevice` with `presentationMode`.
- `app/editor/EditorApp.cpp` — `sceneVN.SetPresentationMode(Offscreen)` after init.

---

## 2026-05-11 · Editor panels render black + `RT_Color0` layout mismatch

**Symptom:** all viewport panels black; validation: `VUID-vkCmdDraw-None-09600` — `RT_Color0` in `COLOR_ATTACHMENT_OPTIMAL`, expected `SHADER_READ_ONLY_OPTIMAL`. Three cascading errors after (`EndQuery` outside renderpass, `pImageIndices-01430`, `acquireNextImageKHR-surface-07783`).

**Cause:** Offscreen pipelines had no end-of-frame layout contract. `Direct` mode transitioned color to `ShaderResource` as a side effect of `BlitMainHdrToDefaultPass`; `Offscreen` mode had no tail pass, so ImGui sampled the texture while still in `ColorAttachment`. First failed draw desynchronised the device → cascade.

**Fix:** new finalising barrier-only pass `PublishOffscreenOutputPass` (declares `MainHdrRenderTarget.color0 → ShaderResource`, `Execute` is no-op). `RasterFeature::OnSchedulePasses` schedules exactly one tail: `BlitMainHdrToDefaultPass` (Direct) or `PublishOffscreenOutputPass` (Offscreen).

**Module contract:** offscreen pipelines guarantee main HDR color is in `ResourceState::ShaderResource` at end-of-frame.

**Files:**
- `eyelib/rendering/framegraph/passes/PublishOffscreenOutputPass.h` — new.
- `eyelib/rendering/framegraph/FrameGraphPassNames.h` — add `kPublishOffscreenOutput`.
- `eyelib/rendering/features/raster/RasterFeature.{h,cpp}` — own + schedule new pass on Offscreen branch.
- Tracked in `Render_steps_todo.md`.

---

## 2026-05-11 · `RenderPipeline::Resize` recreates window swapchain

**Symptom:** every frame: `VUID-VkPresentInfoKHR-pImageIndices-01430` (present image not acquired) + `VUID-vkAcquireNextImageKHR-surface-07783` (acquired count wraps to 4294967295).

**Cause:** `RenderPipeline::Resize(w, h)` called `device->OnWindowResized` when `ownsDevice`. `SceneViewPanel` calls `Resize(panelW, panelH)` every frame with **panel** dimensions, so the swapchain was rebuilt to docked-panel size between `BeginPresentFrame` (which had already done `vkAcquireNextImageKHR`) and `Present`.

**Fix:** `Resize` is render-output-only — updates `outputWidth/Height` + `RenderWorld::SetOutputSize`. Window swapchain stays under the existing `OnWindowResized` entry point (editor main loop drives it explicitly on real framebuffer change).

**Files:**
- `api/rendering/RenderPipeline.cpp` — drop `device->OnWindowResized` from `Resize`.
- `api/rendering/RenderPipeline.h` — docstring: render output only.

---

## 2026-05-11 · Editor lights never reach `SceneLighting` (scene stuck at dark-violet IBL)

**Symptom:** scene view black; after adding a mesh it goes dark-violet (= `IblDefaultProbes` ambient only); adding a `Light::Directional` entity has no visible effect.

**Cause:** `EditorSceneBuilderNew::Build` did not iterate `snapshot.lights`. `RenderScene::directionalLightActive` stayed `false`, so `RenderPipeline::PublishLightsToSceneLighting` left the directional slot untouched and dropped all point + spot entries. The new module's lighting bus only ever saw the default neutral fallback.

**Fix:** Editor adapter now mirrors the player's `WorldRenderBridge` light conversion in one place — walks `snapshot.lights`, fills `scene.directionalLight{.direction/.color/.intensity/.castShadows}` + sets `directionalLightActive = true` for the first directional, and pushes `PointLight` / `SpotLight` PODs (range, cone cosines, castShadows). Direction comes from the entity transform's quaternion via `ForwardFromRotation` (canonical forward = `(0,0,-1)`) — identical convention to the player so editor + player share light shape.

**Files:**
- `app/editor/EditorSceneBuilderNew.cpp` — add `DegreesToRadians` / `ForwardFromRotation` helpers; clear `scene.pointLights` / `scene.spotLights` / `directionalLightActive` at the top of `Build`; new loop over `snapshot.lights` after drawables.
- `app/editor/EditorSceneBuilderNew.h` — docstring: lights now populated per-frame; skybox stays on legacy until per-scene IBL probe swap lands.

---

## 2026-05-11 · Editor drawables have `meshHandle=0` / `materialId=0` (cache never uploads)

**Symptom:** every visible draw row logs `materialId=0 meshHandle=0` plus warn-once `EditorSceneBuilderNew: scene built with placeholder mesh/material handles`. Scene-view + game-view panels stay empty even with mesh + light entities present.

**Cause:** `EditorSceneBuilderNew::Build` was calling `ResourceCache::GetMesh / GetMaterial` directly — those are pure lookups (no upload, no descriptor fetch). The editor never authored any entries into the new cache (only the legacy `EditorGPUResourceCache` was wired, and only into the legacy stack). The player avoids this by routing through two adapters that *upload on miss*: `RendererGPUResourceCacheNew` (legacy `AssetRegistry` descriptor → public POD → `ResourceCache::Create*`) + `BuiltinResourceManagerNew` (primitives + shared default material).

**Fix:** Promote both adapters as runtime-shared (they already live in `OpenEyeGameRuntime`, which the editor links). Docstrings updated to call out the dual client; namespace + file location preserved (single source of truth, zero duplication). Editor now:
1. Constructs both adapters right after `resourceCache` inside the editor's main scope, borrowing `&assetRegistry`.
2. Stashes raw pointers on `EditorContext::gpuResolverNew` + `builtinManagerNew`.
3. `EditorSceneBuilderNew::Build` takes the resolver + builtin instead of the bare cache, and mirrors `WorldRenderBridge::resolveHandles` exactly: built-in mesh ids → builtin manager (mesh + material); file assets → resolver; material miss → `GetDefaultMaterial()` fallback. The placeholder warn-once is gone — the cold path no longer exists.

**Files:**
- `app/player/RendererGPUResourceCacheNew.h` — docstring: runtime-shared, dual client (player + editor).
- `app/player/BuiltinResourceManagerNew.h` — docstring: runtime-shared, dual client.
- `app/editor/EditorContext.h` — forward-declare both adapter classes; add `gpuResolverNew` + `builtinManagerNew` fields.
- `app/editor/EditorApp.cpp` — include both headers; construct adapters after `resourceCache` (inside the ImGui scope so `assetRegistry` is in scope); stash on `ctx`.
- `app/editor/EditorSceneBuilderNew.h` — signature: drop `ResourceCache*`, take `RendererGPUResourceCacheNew*` + `BuiltinResourceManagerNew*`.
- `app/editor/EditorSceneBuilderNew.cpp` — new `resolveHandles` lambda mirroring the player; drop `LogResourceResolutionGapOnce`.
- `app/editor/panels/SceneViewPanel.cpp` + `panels/GameViewPanel.cpp` — pass `ctx->gpuResolverNew` + `ctx->builtinManagerNew` instead of `ctx->resourceCacheNew`.

---

## 2026-05-12 · `EditorDebugLines` + offscreen tail: `vkCmdEndQuery` + layout `09600` (dynamic rendering left open)

**Symptom:** `VUID-vkCmdEndQuery-None-07007` (query started outside pass, end inside / mis-nested), `VUID-vkCmdDraw-None-09600` (HDR `RT_Color0` still `COLOR_ATTACHMENT` while a draw expects `SHADER_READ_ONLY_OPTIMAL`), plus debug `FrameGraph::Compile: attachment-ops contract mismatch` between `GBuffer`/`DeferredLighting` and `EditorDebugLines`.

**Cause:** `vkCmdBeginRendering` from `SetRenderTarget` stayed active through the last geometry pass (`DeferredLighting`, then optional `EditorDebugLines`). `PublishOffscreenOutputPass` replayed barriers **before** its `Execute`, so `vkCmdPipelineBarrier` transitioned HDR colour while it was still bound as a colour attachment; `GpuProfiler::EndFrame` → `vkCmdEndQuery` ran while dynamic rendering was still active relative to validation ordering.

**Fix:** `ICommandList::EndDynamicRendering()` → Vulkan `EndRenderingIfActive`. `FrameGraph::Execute` calls it **after every pass** so the next pass’s barriers never run inside an open dynamic-rendering scope; `BlitMainHdrToDefaultPass::Execute` also ends rendering before the swapchain blit. Removed the overly strict per-texture attachment-op SameOps debug check (shared depth + sequential Clear→Load on HDR produced false positives).

**Files:** `rhi/RHI.h`, `rhi/Vulkan/commands/CommandListVK.{h,cpp}`, `framegraph/FrameGraph.cpp`, `framegraph/passes/BlitMainHdrToDefaultPass.cpp`, `framegraph/passes/PublishOffscreenOutputPass.h` (doc only).

### Follow-up hardening (same incident)

- Reinforced offscreen contract at API boundary: after `RenderWorld::RunFrame`, offscreen `RenderPipeline::Render` now forces `MainHdrRenderTarget.color0 -> ShaderResource` and flushes tracker on the active command list (safety net if tail pass ordering ever changes).
- Guarded Vulkan resize/compose zero-size paths: `DeviceVK::OnWindowResized` ignores `0x0`, and `DeviceVK::BlitToDefaultFramebuffer` returns early when swapchain extent is zero (prevents `vkCreateSwapchainKHR(imageExtent=0)` / zero-width framebuffer/renderpass validation spam during minimize/restore races).
- Files: `api/rendering/RenderPipeline.cpp`, `rhi/Vulkan/core/DeviceVKPresentation.cpp`.

---

## 2026-05-11 · Editor debug-line overlay (grid / axes / colliders) lands on the new pipeline

**Symptom:** new-pipeline editor branch (`useNewPipeline`) had no scene grid, no XYZ axes, no collider helpers — `SetPendingDebugLinesColored` was a paused stub on `api/rendering`, so the call was silently dropped. ImGuizmo / selection still worked (pure ImGui-overlay) but the world looked floating.

**Cause:** `eyelib/rendering/features/editor/{EditorFeature,DebugDrawPass}.{h,cpp}` were one-line scaffolds. The api/rendering façade kept three paused stubs (`RenderDebugLines`, `SetPendingDebugLines`, `SetPendingDebugLinesColored`) — each just logged a one-time warning and returned.

**Fix:** First real consumer of the `features/editor/` slot (P9 "editor as a feature"). Module-side:

- `features/editor/DebugLine.h` — POD `{Vec3 a, Vec3 b, Vec4 color}` (frame-local; copied into the feature on `SetLines`).
- `features/editor/DebugDrawPass.{h,cpp}` — owns 3-slot host-visible vertex ring (`BufferUsage::Vertex` + `BufferMemoryHint::HostVisible`); maps + fills via `IBuffer::Map/Unmap`; binds `Frame` descriptor set (PerView UBO at `set=0,binding=1`); reads existing depth (`MainDepth → DepthRead`) for depth-tested overlay; writes `MainHdrRenderTarget` with `loadOp=Load`; `LineList` topology; depth test enabled but no depth write; cull disabled.
- `features/editor/EditorFeature.{h,cpp}` — `IRenderFeature` with `SchedulingOrder = 100` so it runs strictly after `RasterFeature` (overlay on top of deferred-lit HDR). Registers shader permutation `EditorDebugLines` in `OnRegister`; `OnSchedulePasses` short-circuits when `m_pendingLines.empty()` so unused frames carry zero pass cost.
- `framegraph/FrameGraphPassNames.h` — `kEditorDebugLines`.
- `framegraph/FrameGraphResourceNames.h` — `writer_names::kEditorFeature` (debug name only; feature is consume-only).
- `shaders/ShaderPermutationKey.h` — add `ProgramId::EditorDebugLines`.
- `assets/shaders/rendering/vulkan/debug/debug_lines.{vert,frag}` — new pair (vert reads `uPerView.uViewProjection`, frag passes interpolated `vColor`).

Public surface:

- `api/rendering/RenderPipeline.cpp` — activate all three previously-paused stubs (`RenderDebugLines`, `SetPendingDebugLines`, `SetPendingDebugLinesColored`); converts the legacy pair shape into `eyelib::rendering::DebugLine[]` and pushes via `EditorFeature::SetLines`. Lines auto-clear after each `Render(scene)` so callers that stop feeding the surface do not see stale geometry (single-frame contract preserved). `Impl::warnedDebugLinesPaused` removed.
- `Impl` owns `std::unique_ptr<EditorFeature> editor`, subscribed unconditionally after raster + shadow.

Editor wiring:

- `app/editor/panels/SceneViewPanel.cpp` — `useNewPipeline` branch now builds the same `ColoredLines` the legacy branch builds (`CollectGridAndAxisLinesColored` + `CollectColliderDebugLines`) and calls `sceneViewPipelineNew->SetPendingDebugLinesColored` right before `Render(newScene)`.

ImGuizmo: no code change. It already worked on the new pipeline (purely ImGui draw-list overlay — `ImGuizmo::BeginFrame` already runs in the new-pipeline branch at `EditorApp.cpp:622` and `ImGuizmo::Manipulate` calls `ImGui::GetWindowDrawList()`).

