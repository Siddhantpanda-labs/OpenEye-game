# Vulkan Fix Plan and Implementation

## Objective

Fix Vulkan mesh invisibility and layout/descriptor instability with an explicit, backend-correct rendering contract:

- Correct Vulkan clip/depth conventions for camera projection.
- Prevent descriptor lifetime hazards during multi-mesh drawing.
- Ensure post/outline image layout transitions are deterministic and valid.
- Keep OpenGL behavior unchanged.

## Root Causes Identified

1. Descriptor update-after-bind hazard in forward/forward+ material binding (single material set updated per draw).
2. Render target copy path missing transfer destination capability on Vulkan color RTs used as copy destinations.
3. Outline/post image layout mismatches around `RT_Color0` and sampled descriptor expectations.
4. Backend convention mismatch: Vulkan camera projection consumed with GL clip-space assumptions.

## Implemented Changes

### 1) Descriptor Lifetime Safety (Forward/Forward+)

- Replaced single mutable material descriptor usage in draw loops with per-frame per-draw descriptor sets.
- Added a frame-local descriptor pool/cursor and draw-time acquisition path.
- Bound the draw-local material set for each draw call.

Result: no update-after-bind invalidation when multiple meshes/materials are rendered in one command buffer.

### 2) Vulkan Render Target Copy Usage Contract

- Added `TransferDst` usage to non-transient Vulkan render-target color textures (including shared-depth color RT path).

Result: `vkCmdCopyImage` and `TRANSFER_DST_OPTIMAL` transitions are legal for RT color copies/blits.

### 3) Outline/Post Layout Determinism

- Ensured `RenderOutlineFromIdBuffer()` transitions final HDR output back to shader-read before UI sampling.
- Updated Vulkan blit restore policy so destination copy target is restored to shader-read for post/outline consumption.

Result: descriptor image layout and actual image layout remain consistent at sampling time.

### 4) Vulkan Camera Convention Normalization

- Added backend-normalization in renderer boundary:
  - Vulkan only: apply projection correction matrix for explicit clip/depth convention.
  - OpenGL and others: unchanged.
- Normalization now applied in:
  - `PrepareFrameData()` camera pipeline path.
  - Stereo camera setup (`m_currentLeftCam`, `m_currentRightCam`).
  - Direct skybox rendering entry path (`RenderSkybox(view, ...)`).
  - Stereo post/debug draw camera usage.

Result: forward/deferred/stereo/skybox paths consume a consistent Vulkan projection convention from a single renderer boundary policy.

## Design Notes

- Convention policy is centralized in renderer code rather than scattered in gameplay/editor camera builders.
- Vulkan-specific behavior is isolated by backend check (`GetShaderBackendName()`), preserving OpenGL parity.
- Passes continue to use the same camera data model (`CameraView`) while backend correction is applied once in render path.

## Verification Checklist

Use this checklist per build:

- [ ] Add first mesh: visible in final image.
- [ ] Add second mesh: remains visible; no descriptor update/recording invalidation.
- [ ] No repeated `VUID-VkDescriptorImageInfo-imageLayout-00344` / `08114`.
- [ ] Outline selection renders correctly with no layout errors.
- [ ] Skybox and meshes coexist correctly (no full-sky overwrite when opaque exists).
- [ ] Stereo path renders geometry in both eyes.
- [ ] OpenGL path behavior unchanged.

## Remaining Follow-up (Optional Hardening)

- Move all ad-hoc texture state changes into stricter framegraph-declared read/write transitions where practical.
- Add debug assertions for backend camera normalization and expected output texture state before UI sampling.
- Add cross-backend visibility/depth regression test scenes (opaque overlap, skybox, shadowed mesh).
