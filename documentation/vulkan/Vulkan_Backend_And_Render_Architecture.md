# Vulkan Backend and Render Module Architecture

This document describes the **current Vulkan backend** and how it integrates with the render module in OpenEye.

It is focused on what exists in code today, the intended architecture contracts, and how frame execution flows across renderer, frame graph, RHI, and Vulkan internals.

---

## 1) Scope and Design Intent

The Vulkan renderer follows these principles:

- **Resource-space descriptor ABI**: fixed set/binding contracts shared by shaders and C++ descriptor writes.
- **Explicit ownership**: `RenderPipeline` orchestrates passes; RHI backend owns Vulkan objects and submission.
- **Frame graph first**: Deferred / Forward / Stereo modes compile and execute pass DAGs; transitions happen via explicit resource states.
- **Backend abstraction**: higher layers target `rhi::IDevice` and `rhi::ICommandList`; Vulkan implements details.
- **No hidden global rendering state**: command recording is explicit, and backend tracks barriers/layouts.

Primary code locations:

- Render orchestrator: `EngineSource/eyelib/render/renderer/RenderPipeline.cpp`
- Render module overview: `EngineSource/eyelib/render/RENDER_MODULE.md`
- Vulkan device/core: `EngineSource/eyelib/render/rhi/Vulkan/core/DeviceVK.cpp`
- Vulkan command recording: `EngineSource/eyelib/render/rhi/Vulkan/commands/CommandListVK.cpp`
- Vulkan pipeline creation: `EngineSource/eyelib/render/rhi/Vulkan/pipelines/PipelineVK.cpp`
- Resource-space layouts: `EngineSource/eyelib/render/rhi/Vulkan/pipelines/VulkanResourceSpaces.cpp`
- Descriptor ABI contract: `EngineSource/eyelib/render/rhi/Vulkan/descriptor_binding_table.md`

---

## 2) Layered Architecture

### 2.1 High-level layers

1. **Scene/API layer**
   - Exposes `IRenderSceneView`-style scene access.
   - Produces frame inputs (camera, lights, visible drawables, skybox).

2. **Render orchestration layer**
   - `RenderPipeline` builds per-frame data, prepares pass resources, compiles and executes frame graph recipes.
   - Handles mode-specific sequencing (Deferred, Forward, Forward+, Stereo).

3. **Pass layer**
   - Dedicated passes (`GBufferPass`, `ShadowPass`, `LightingPass`, post-processing, transparent, skybox, outline/entity ID).
   - Uses `RenderContext` + `rhi::ICommandList`.

4. **RHI abstraction layer**
   - Interfaces in `RHI.h` and `RHITypes.h`.
   - Backend-agnostic commands: bind pipeline, set RT, barriers, draw/dispatch, descriptor sets.

5. **Vulkan backend layer**
   - `DeviceVK`, `CommandListVK`, Vulkan resource objects, descriptor sets, pipeline layouts, barrier tracker, swapchain/presentation.
   - Handles Vulkan object lifecycle, synchronization, layout transitions, descriptor allocation/writes.

### 2.2 Why this split matters

- Renderer code can stay API-driven and avoid backend branches.
- Vulkan-specific correctness (layouts, descriptor validity, dynamic rendering compatibility) is isolated in backend + ABI contracts.
- Pass sequencing can evolve through frame graph recipes without rewriting backend internals.

---

## 3) Core Runtime Data Flow

### 3.1 Frame data preparation

`RenderPipeline` prepares immutable per-frame state:

- camera (`CameraView`)
- directional light (`LightParams`)
- point/spot lights
- visible opaque/transparent sets
- all opaque for shadow submission

This is used by pass executors and avoids repeated scene traversal.

### 3.2 Frame graph execution path

For graph-based modes:

1. Import persistent textures (HDR, GBuffer attachments, shadow map, etc.).
2. Define recipe (pass names, reads, writes, side effects).
3. Resolve pass executors by name.
4. Compile graph for current dimensions/mode/stereo.
5. Execute in dependency order.

Frame graph coordinates transitions between pass read/write states through RHI barriers.

### 3.3 Command recording and submission

- Renderer records through `ICommandList` (backed by `CommandListVK`).
- Backend tracks active dynamic rendering scope and ends it before barriers when required.
- Submission is device-owned (`BeginFrame` / `EndFrame` / `Present`).

---

## 4) Descriptor and Resource-Space ABI

The Vulkan ABI is explicitly defined in `descriptor_binding_table.md`.

### 4.1 Set model (current contract)

- `set 0`: Frame
- `set 1`: Pass-local resources
- `set 2`: Material
- `set 3`: Bindless heap
- `set 4`: Scene buffers

Standalone layouts also exist for specialized pipelines (`IBL`, `OutlinePost`, etc.).

### 4.2 Layout factories

Layout creation is centralized in `VulkanResourceSpaces.cpp`:

- `CreateFrameSetLayout`
- `CreateForwardPassSetLayout`
- `CreatePostPassSetLayout`
- `CreateShadowPassSetLayout`
- `CreateDeferredLightingPassSetLayout`
- `CreateSceneBuffersSetLayout`
- `CreateOutlinePostSetLayout`

Pipeline layouts for each `PipelineLayoutKind` are built from these set layouts.

### 4.3 Descriptor writes

`DescriptorSetVK` performs typed writes:

- `UpdateTexture`
- `UpdateUniformBuffer`
- `UpdateStorageBuffer`

Safety behavior includes:

- Declared binding type validation against layout descriptors.
- Binding completeness tracking (`IsCompleteForBind`).
- Image layout declared as `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` for sampled image descriptors.

Recent architectural behavior:

- Integer ID textures (`R32UI`) now use nearest sampling path through Vulkan sampler selection.

---

## 5) Vulkan Resource and State Management

### 5.1 Device initialization

`DeviceVK::Initialize` creates and wires:

- Vulkan instance/device/queues/swapchain
- descriptor pools and descriptor allocators
- default pipeline layouts and resource-space layouts
- timeline semaphore, upload heap, descriptor cache
- immediate command list and per-frame contexts

### 5.2 Texture and render target objects

- `TextureVK`: wraps image/imageView/allocation + format/type metadata.
- `RenderTargetVK`: wraps color/depth attachments and Vulkan attachment views.

### 5.3 Barrier tracking model

`BarrierTrackerVK` tracks expected per-resource states and queues pending transitions.

`DeviceVK::TransitionTextureToState` records desired state.
`DeviceVK::FlushBarrierTracker` emits actual Vulkan barriers via `vkCmdPipelineBarrier`.

Important backend rule:

- If barriers are issued during active dynamic rendering scope, command list ends rendering first to remain Vulkan-valid.

---

## 6) Dynamic Rendering and Pipeline Compatibility

The backend uses dynamic rendering (`vkCmdBeginRendering`) rather than legacy render passes for pass recording.

Pipeline creation includes `VkPipelineRenderingCreateInfo`:

- color attachment formats
- depth/stencil attachment format

These must match actual render target attachments bound at draw time.

Standard attachment format sets are defined in `PipelineAttachmentFormatSets.h`, used to keep pipeline and runtime RT formats aligned.

---

## 7) Pass Architecture in Vulkan Path

### 7.1 Shadow

- `ShadowPass` renders cascades to depth array target.
- Cascade map output is consumed by lighting/forward shaders as sampled depth.
- Scene lighting header includes shadow controls and cascade metadata.

### 7.2 Deferred

- GBuffer writes position/normal/albedo/PBR/depth.
- Lighting pass samples GBuffer + shadow + scene light SSBO + optional IBL.

### 7.3 Forward and Forward+

- Forward opaque path uses scene-light SSBO + shadow map + material textures.
- Forward+ path includes clustered/tiled data plumbing and scene-local light loops.

### 7.4 Post and editor overlays

- Post stack consumes HDR output and writes final output RT.
- Editor-related passes (entity ID, outline composite) are implemented as explicit additional passes over renderer outputs.

---

## 8) Current Known Architectural Notes

1. **Descriptor ABI discipline is mandatory**
   - Shader declarations, layout factories, and C++ descriptor updates must stay synchronized.

2. **Layout/state transitions are central**
   - Validation failures usually indicate missing pass-boundary transitions or incorrect tracked state restoration.

3. **Per-frame descriptor usage**
   - Descriptor sets updated every frame for dynamic resources should be frame-indexed to avoid in-flight update hazards.

4. **Sampler correctness by texture type**
   - Integer sampled IDs require nearest sampling; color textures can use linear.

5. **Blit/copy paths are part of render correctness**
   - RT->RT copies used by post/composite features must preserve and restore tracked image states.

---

## 9) Render Module Integration Summary

From render module perspective, Vulkan backend is not a separate renderer; it is the concrete RHI implementation under the same render architecture.

Integration points:

- `RenderPipeline` and passes stay RHI-oriented.
- Vulkan backend owns:
  - descriptor set internals
  - dynamic rendering begin/end and pipeline compatibility
  - barriers and image layouts
  - queue submission and present
- Frame graph/pipeline-level architecture remains backend-agnostic while Vulkan enforces explicit correctness constraints.

---

## 10) Maintenance Guidelines

When changing Vulkan rendering behavior:

1. Update descriptor ABI doc and layout factories together.
2. Verify pipeline attachment format selections for any pass format changes.
3. Ensure state restoration in copy/blit/resolve helper paths.
4. Keep per-frame mutable descriptor usage frame-safe.
5. Run validation-enabled test scenes for:
   - first mesh add
   - multi-mesh scenes
   - entity ID + outline path
   - deferred + forward mode switches

---

## 11) Suggested Companion Docs

- ABI contract: `EngineSource/eyelib/render/rhi/Vulkan/descriptor_binding_table.md`
- Render module overview: `EngineSource/eyelib/render/RENDER_MODULE.md`
- Refactor roadmap: `documentation/refractor/Render_refractor.md`
- Efficiency roadmap: `documentation/refractor/Pipeline_efficiency_roadmap.md`

