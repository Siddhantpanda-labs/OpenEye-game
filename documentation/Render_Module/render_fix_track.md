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
