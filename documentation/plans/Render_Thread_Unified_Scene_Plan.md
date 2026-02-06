# Render Thread + Unified Scene Plan

**Goal:** Make the runtime rendering architecture **robust and professional** by enforcing strict thread ownership, immutable render input, unified scene types, and zero‑copy (span/view) data flow.

This plan builds on the current Phase 3b work and removes the remaining structural flaws (render thread ownership, resize correctness, API→internal copies, shared mutable render state).

---

## Current status (how far we are)

| Phase | Name | Status |
|-------|------|--------|
| 0 | Freeze the Thread Model | **Done** |
| 1 | Unified RenderScene Type | **Done** |
| 2 | Span/View-Based Render Input | **Done** |
| 3 | Frame Allocator as Snapshot Owner | **Done** |
| 4 | Immutable Snapshot of All Render Inputs | **Done** |
| 5 | Render Command Queue (Resize + GPU Events) | **Done** |
| — | **Robust presentation (post–Phase 5)** | **Done** (see below) |
| 6 | Asset & Shader Loading Discipline | Not started |
| 7 | Robust Resize Handling | Not started |
| 8 | Optional: Triple Buffer + Frame Pacing | Not started |

**We are at: Phase 5 complete + robust presentation; next is Phase 6.**

---

## Summary of Key Principles

1. **Render thread owns all GL and GPU resources.**
2. **Game thread builds an immutable render snapshot.**
3. **No shared mutable render state between game and render threads.**
4. **Unified RenderScene type used end‑to‑end.**
5. **Zero‑copy or allocator‑backed snapshots (no heap churn).**
6. **Resize and GPU resource creation are render‑thread commands only.**

---

## Phase 0 — Freeze the Thread Model (Baseline Rules)

**Goal:** Enforce strict ownership boundaries to avoid hidden races.

**Rules:**
- All **GL calls** and **GPU resource creation** must happen on the **render thread**.
- Game thread must **never** call GL, even indirectly.
- Render thread must **never** read from World/animation/components directly.

**Deliverables:**
- Render thread is the single owner of GL context (explicit MakeContextCurrent on render thread).
- Resize callback does **not** touch GL or render resources.
- Any GL call from main thread becomes a hard error (optional debug assert).

**Status (done):**
- Render thread calls `MakeContextCurrent()` in the render loop; game thread never touches GL after warmup.
- Resize: `FramebufferResizeCallback` only updates window dimensions and invokes optional client callback; contract documented in `SetResizeCallback` and in the callback implementation that the callback must not use GL or render resources. Actual resize (e.g. pipeline FBOs) is done on the render thread in `RenderSceneOnly` when dimensions change.
- Optional main-thread GL assert deferred; rules documented here and in code comments.

---

## Phase 1 — Unified RenderScene Type (Eliminate API→Internal Copy)

**Problem:** API → internal conversion (`api::RenderPipeline::Render`) copies scene every frame.

**Target:** Use a **single RenderScene type** across API and eyelib to eliminate per‑frame copies.

**Work:**
- Move/merge `eyelib::render::RenderScene` and `eyelib::api::RenderScene` into **one shared type**.
- Ensure `RenderPipeline` consumes this unified type directly.
- Remove `ToInternal(scene)` conversion path.

**Result:** Render thread receives the **same scene instance** that the game thread built (via shared allocator or spans).

**Status (done):**
- **Unified type at API boundary:** `api::RenderScene` is the single type passed from game thread; `api::RenderPipeline::Render(scene)` now passes `scene` directly to the internal pipeline (no conversion in api layer).
- **Internal pipeline** accepts `api::RenderScene` via overload `eyelib::render::RenderPipeline::Render(const api::RenderScene&)`. Conversion from API to internal is done once per frame in `FillScratchFromApiScene()` into a **reused scratch** `m_scratchScene` (no per-frame heap; same scene reference at the API).
- **RenderSystem (ECS) path** unchanged: still builds `eyelib::render::RenderScene` and calls `Render(internal scene)` overload.
- **ToInternal(scene)** removed from `api/render/RenderPipeline.cpp`; conversion lives only inside `eyelib/render/RenderPipeline.cpp` for the api overload.

---

## Phase 2 — Span/View‑Based Render Input

**Goal:** Use spans/views for render input and avoid allocations/copies.

**Work:**
- RenderScene stores **spans/views** over frame data:
  - `span<Drawable>` (opaque/transparent)
  - `span<PointLight>`, `span<SpotLight>`
  - span for bone matrices per drawable
- The backing memory lives in a **frame allocator** or per‑frame arena.

**Result:** Render thread reads **zero‑copy** data with stable lifetime for that frame only.

**Status (done):**
- **No scratch copy:** Pipeline no longer converts `api::RenderScene` into an internal `RenderScene`. `Render(api::RenderScene)` uses the API scene directly: `BuildAndExecuteFrameGraph(api)`, `RenderForward(api)`, `RenderForwardPlus(api)`, `RenderSkybox(api)` all take `const api::RenderScene&` and iterate over `scene.opaque` / `scene.transparent` / `scene.pointLights` / `scene.spotLights` in place.
- **Span-based pass API:** Pass overloads that accept API data take `std::span<const api::Drawable>` or `std::span<const api::render::PointLight/SpotLight>`, so frame-allocator-backed vectors (custom allocator) are passed as `span(data(), size())` with no copy or allocator mismatch.
- **Resolve at use:** For each drawable the pipeline/passes call `GetInternal().get()` on mesh/material/materialInstance only when needed for binding/drawing; no bulk conversion of drawable arrays.
- **Forward+ lights:** Point/spot lights are still converted once into `m_scratchPointLights` / `m_scratchSpotLights` for SSBO upload (std140 layout); drawables remain zero-copy.

---

## Phase 3 — Frame Allocator as the Snapshot Owner

**Goal:** Make frame data memory deterministic and reset each frame.

**Work:**
- Add one **FrameAllocator per render slot** (double/triple buffer).
- Game thread resets allocator at the start of each slot build.
- All snapshot data (drawables, lights, bones, per‑frame params) allocated from that allocator.

**Result:** Fast, stable allocations; no heap churn; explicit frame‑lifetime ownership.

**Status (done):**
- **One FrameAllocator per slot:** `RenderFrameSync::FrameSlot` holds `api::FrameAllocator` (1 MiB default) and `api::RenderScene scene(&allocator)`. Double-buffered (`kBufferCount = 2`).
- **Reset at slot start:** `GetFillSlot()` calls `m_slots[slot].Reset()` which runs `allocator.Reset()` and reinitializes `scene` with the same allocator, so each fill uses a clean allocator.
- **Snapshot data from allocator:** `RenderScene` uses `FrameAllocatorAdaptor` for `opaque`, `transparent`, `pointLights`, `spotLights`, so all `push_back` use the frame allocator. `BuildScene(..., frameAlloc)` allocates bone matrices via `frameAlloc->AllocateArray<Mat4>()` when provided. PlayerApp always passes `GetFrameAllocator(slot)` into `BuildScene`.

---

## Phase 4 — Immutable Snapshot of All Render Inputs

**Goal:** The render thread reads **only immutable per‑frame data**.

**Work:**
- Move all per‑frame render state into the snapshot:
  - Bone matrices (already done)
  - Material overrides (albedo/metallic/roughness/alpha/vec3/vec4)
  - Per‑drawable parameters like UV transform, flip, etc.
  - Any script‑driven render params
- Ensure render thread never touches live `MaterialInstance` or mutable game state.

**Result:** No data races, flicker, or shared mutable objects.

**Status (done):**
- **DrawableRenderParams:** New struct in `api/render/RenderScene.h` holds albedo, metallic, roughness, ao, emission, alpha, uvTransform, uvRotation, texCoordIndex, flipV, and the seven standard texture slots (albedo, normal, metallicRoughness, ao, emissive, metallic, roughness). Each `api::Drawable` has a `params` member.
- **Fill at build time:** `FillDrawableRenderParams(out, material, materialInstance)` in `api/render/DrawableSnapshot.cpp` fills the snapshot from material + optional materialInstance (game thread only). `WorldRender::BuildScene()` calls it for every opaque drawable before pushing to the scene.
- **Render thread uses only snapshot:** Forward, ForwardPlus, GBufferPass, ShadowPass, and TransparencyPass (api overloads) no longer call `materialInstance->Bind()`, `GetAlbedo()`, `GetTexture()`, or `ForEachMat4Array()`. They use `apiD.params` for all PBR/UV/texture data and `apiD.material->GetInternal()` only for the shader; bones come only from `apiD.bones` / `apiD.boneCount`.
- **Bones:** Render thread reads bones only from `drawable.bones`; when `frameAlloc` is present, `BuildScene` already copies bones into the frame allocator. No live `MaterialInstance` reads.

---

## Phase 5 — Render Command Queue (Resize + GPU Events)

**Goal:** All render‑thread operations are explicit commands.

**Work:**
- Introduce a small **RenderCommandQueue**:
  - `RenderFrame(sceneSnapshot)`
  - `Resize(width, height)`
  - `ReloadShaders(...)`
  - `Shutdown`
- Game thread **enqueues commands only**.
- Render thread processes commands in order.

**Result:** No hidden resize calls or GL side‑effects outside render thread.

**Status (done):**
- **Resize command queue:** `RenderFrameSync` has `EnqueueResize(width, height)` (game thread) and `TakeResizeCommands()` (render thread). Pending resizes stored in `m_resizeQueue`. Size is now taken on render thread via `RefreshFramebufferSize()` (see Robust presentation below).
- **Game thread:** In `PlayerApp` update, when `window.Width()`/`Height()` change, `frameSync.EnqueueResize(w, h)` is called before building the frame. No GL or pipeline calls on the game thread.
- **Render thread:** After `AcquireFrame()`, the render thread calls `TakeResizeCommands()`, coalesces to the latest (w, h), and calls `WorldRenderBridge::ApplyResize(w, h)` (which calls `m_pipeline.Resize` on the render thread). Then `RenderSceneOnly(scene, frame.width, frame.height)` runs; it still applies a fallback resize if dimensions don’t match (first frame or missed event).
- **Shutdown:** Already explicit via `SignalShutdown()` and `AcquireFrame()` returning nullopt; no separate shutdown command added.
- **RenderFrame:** Frame payload remains (slot, width, height, overlay) via `SubmitFrame`/`AcquireFrame`; no separate `RenderFrame` command type. `ReloadShaders` left for Phase 6 or later.

---

## Robust presentation (post–Phase 5, implemented)

**Goal:** No flicker or black frames from wrong size, wrong buffer, or invalid post-process output.

**Done:**
- **Single source of truth for size:** Render thread calls `Window::RefreshFramebufferSize()` every frame (thread-safe `glfwGetFramebufferSize`); all resize and render use this size. Game thread no longer enqueues resize; render thread owns size.
- **Zero size = no render, no present:** If `fbWidth <= 0` or `fbHeight <= 0`, render thread skips `ApplyResize`, `RenderSceneOnly`, and `SwapBuffers`; only `ReleaseFrame(slot)` so game thread does not block.
- **Vsync re-applied every frame:** `Window::ApplyVsync()` is called on the render thread each frame (with context current) so the driver cannot drop vsync.
- **Present only when a valid frame was rendered:** `RenderSceneOnly` returns `bool`; `SwapBuffers()` and overlay are only called when it returns true. Pipeline exposes `HasValidOutput()`; `RenderSceneOnly` returns false if pipeline has no valid FBO.
- **Post-process output guarded:** `PostProcessor::HasValidOutputFrame()` is set true only when tone mapping actually draws to the output FBO. `BlitToScreen()` uses post-process output only when `GetOutputTexture() != 0` and `HasValidOutputFrame()`; otherwise it blits from the HDR FBO so we never present a black post-process buffer.
- **Pipeline zero-size guards:** `Render(api::RenderScene)` and `Render(RenderScene)` early-return when `m_width == 0` or `m_height == 0`; `Resize` and `CreateHDRFramebuffer` already rejected zero size.

**Files touched:** `eyelib/platform/window/Window` (RefreshFramebufferSize, ApplyVsync), `api/platform/Window`, `app/player/PlayerApp.cpp` (render loop), `app/player/WorldRender` (ApplyResize, RenderSceneOnly returns bool), `eyelib/render/RenderPipeline` (HasValidOutput, zero guards, BlitToScreen post-process guard), `eyelib/render/PostProcessor` (HasValidOutputFrame), `api/render/RenderPipeline` (HasValidOutput).

---

## Phase 6 — Asset & Shader Loading Discipline

**Goal:** GPU resource creation occurs only on render thread.

**Work:**
- Split asset loading into **CPU‑only** (game thread) and **GPU upload** (render thread).
- Materials/shaders should be **compiled on render thread** only.
- ShaderWatcher events become render‑thread commands.

**Result:** Stable GPU resource creation and predictable threading.

---

## Phase 7 — Robust Resize Handling

**Goal:** Resize never produces invalid or black frames.

**Work:**
- Resize event becomes a render command.
- If size is zero, skip resource rebuild and **pause rendering**.
- Resume once valid size is received and resources are rebuilt.

**Result:** No black frames or incomplete FBOs.

---

## Phase 8 — Optional: Triple Buffer + Frame Pacing

**Goal:** Smooth pacing and eliminate stalls.

**Work:**
- Use **triple buffering** so the game thread never blocks while render thread finishes.
- FrameSync manages N slots with proper back‑pressure.

**Result:** Stable frame pacing and predictable latency.

---

## Final Architecture (End State)

**Game Thread**
- Update world → build render snapshot → enqueue RenderFrame command

**Render Thread**
- Process commands (Resize, RenderFrame, ReloadShaders, Shutdown)
- Owns all GL resources
- Reads only immutable snapshot data

**Memory**
- Per‑frame allocator
- Spans/views into frame storage

---

## Notes / Migration Guidance

- Phases 1–4 are the minimum to make the architecture sound and flicker‑free.
- Phases 5–7 are needed for production‑quality robustness (resize, hot reload, correct GL ownership).
- Phase 8 is optional for performance/pacing.

---

## Outcome

Once complete, the render system is:
- Thread‑safe
- Deterministic
- Zero‑copy (or allocator‑backed)
- Architecturally clean and scalable

