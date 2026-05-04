# Render fix track — deferred path resolved

## Resolution

**Status:** Fixed. The sandbox cube is visible in both **Forward** and **Deferred** mode.

**Root cause:** `RenderResources` reused staging buffer offset `0` for every upload recorded into the same command buffer. Per-frame, per-view, directional-light, and per-object uploads all copied from the same staging source address, while the CPU overwrote that address before GPU execution. That made `uPerView` / the per-object SSBO read as missing or garbage, so forward and G-buffer rasterization could produce no visible cube.

**Final fix:** `RenderResources` now allocates a unique staging-buffer slice for each upload during `BeginUploadRecordingPhase()` → `EndUploadRecordingPhase()`. Uniform, SSBO, and scratch uploads write/copy from their own source offsets.

**Verification:** `cmake --build build --config Debug --target OpenEyeRenderingSandbox` succeeds. User confirmed the cube is visible in **Forward** and **Deferred**.

---

## Cleanup Completed

Temporary deferred debug instrumentation has been reverted:

- Deleted `EngineSource/eyelib/rendering/core/DeferredDebugVis.h`.
- Restored normal G-buffer clear color `{0,0,0,1}` in `GBufferPass.cpp`.
- Restored normal deferred HDR clear color `{0.1,0.15,0.2,1}` in `DeferredLightingPass.cpp`.
- Removed `[DeferredDebug]` logs and counters from `GBufferPass.cpp`, `DeferredLightingPass.cpp`, and `RasterFeature.cpp`.
- Removed `OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE` probe branches from `deferred_lighting_analytic.frag`.
- Removed earlier one-shot forward/raster diagnostic logs now that upload staging is fixed.

---

## Permanent fixes kept

1. `DeviceVK::GetResourceSetLayout` — use `sizeof(m_resourceSetLayouts) / sizeof(...)` instead of `i < 11` so `RasterDeferredLightingData` resolves. Do **not** revert that.
2. **D24S8 sampled depth (VUID-VkDescriptorImageInfo-imageView-01976):** `TextureVK` creates an extra **depth-only** `VkImageView` (`m_depthSrvImageView`); `GetVkImageViewForShaderSamplerBinding()` selects it for `DescriptorSetVK::UpdateTexture`, bindless, and `VulkanBindingUtils`. The primary `m_imageView` stays **depth+stencil** for attachments and barriers. Do **not** revert without replacing with an equivalent spec-correct path.
3. **Deferred lighting sampled wrong depth:** `DeferredLightingPass` must bind **`GBufferRenderTarget` depth**, not `MainHdrRenderTarget` depth (HDR depth is not written during G-buffer). Wrong binding left depth at clear (1.0) so the analytic shader always took the sky branch.
4. **Stale deferred lighting descriptors:** `DeferredLightingPass` cached pass descriptors with a key that omitted **G-buffer color texture pointers**. After resize / `RenderResources::EnsureAllocated`, new MRT images could be created while shared depth stayed the same → descriptors still sampled **old/freed** color attachments (flat / garbage). Fix: fold `GetColorTexture(0..2)` (and depth) into `dataKey`, or refresh textures every frame.
5. **`DepthRead` must synchronize shader sampling:** `ResourceState::DepthRead` in `EngineSource/eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h` includes **`VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT`** and **`VK_ACCESS_SHADER_READ_BIT`** (in addition to depth-test stages/access) so `texture(tDepth)` after G-buffer is valid. **`DeviceVKPresentation.cpp`** depth layout helper updated to match.

---

## Final diagnosis notes

| Area | Final verdict | Notes |
|------|---------------|-------|
| Schedule / pass order | OK | Deferred mode schedules **GBuffer → DeferredLighting → Blit**. |
| G-buffer raster path | Fixed by upload slices | The clear-only G-buffer observation was caused by bad per-view / per-object data from staging offset reuse, not by the shader math itself. |
| Deferred depth binding | Fixed and kept | Deferred lighting samples `GBufferRenderTarget` depth, not Main HDR depth. |
| Deferred descriptors | Fixed and kept | Descriptor cache key includes G-buffer color textures and depth. |
| Depth read synchronization | Fixed and kept | `DepthRead` includes fragment shader + shader-read access. |
| Fullscreen UV | OK | `fullscreen_triangle.vert` emits `[0,2]`; deferred lighting uses `vUv * 0.5`. |
| Stale SPIR-V / dual asset roots | Not the tracked cause | User asked to keep this out of scope; the confirmed fix was runtime upload staging. |

The key lesson is that RenderDoc showing the draw/mesh was not enough: the shader inputs were being corrupted before execution because all recorded uploads referenced the same staging source bytes.

---

## Changelog

- **2026-05-03:** Document created; deferred pass-color + log instrumentation added per table above.
- **2026-05-03:** Depth/stencil descriptor fix: depth-only SRV view + `GetVkImageViewForShaderSamplerBinding()` (see permanent fixes above).
- **2026-05-03:** `DeferredLightingPass`: sample **G-buffer** depth + barriers on that texture (not Main HDR depth).
- **2026-05-04:** `DeferredLightingPass`: descriptor cache key now includes **G-buffer color texture pointers** (fixes stale sampling after RT realloc).
- **2026-05-04:** RenderDoc: flat G-buffer MRT0 matching debug clear → **(A) confirmed**; doc table **“Where the bug lives in code”** added; matrix row **fragments / depth** updated from “open” to **active on raster path**.
- **2026-05-05:** Added **Hypothesis narrowing** section: code-reviewed matrix vs deprecated `eyelib/render` (reference only); documented remaining **open** item (**fragment-level raster/depth**) and **diagnostic** limit of mesh-only RD inspection; stale SPIR-V / dual-asset marked **out of scope** per user.
- **2026-05-05:** Changelog note: permanent item **#5** — `DepthRead` barrier includes **FS + `SHADER_READ`** for sampled depth (`VulkanEnums.h` rendering + `DeviceVKPresentation.cpp`).
- **2026-05-05:** Continued review: documented **upload→execute** ordering, **PerObject SSBO** barriers (VS-visible), **dynamic depth** refresh on `BindPipeline`, marked **#6** / dual-asset as **out of scope** per user; added matrix **#9** (ordering).
- **2026-05-05:** Added **Localization snapshot**: clarifies we are **not** in a fully unknown space — residual risk is **G-buffer raster (A)** vs **deferred read (B)** fork; ~70–85% of pipeline surface reviewed or fixed in code.
- **2026-05-04:** Final root cause fixed: `RenderResources` now uses unique staging-buffer source slices per recorded upload; forward and deferred cube visibility confirmed.
- **2026-05-04:** Temporary deferred debug instrumentation removed: debug clear header deleted, `[DeferredDebug]` pass logs/counters removed, and deferred shader probe modes removed.
- **2026-05-04:** Removed remaining temporary forward/raster diagnostic logs from `ForwardOpaquePass` and `RasterFeature`.
