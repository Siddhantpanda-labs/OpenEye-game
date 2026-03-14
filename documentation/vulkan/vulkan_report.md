# Vulkan Backend Report

This document describes the **current structure and behavior** of the OpenEye Vulkan RHI backend: file roles, data flow, and how each component fits into the pipeline.

---

## 1. Overview

The Vulkan backend lives under `EngineSource/eyelib/render/rhi/Vulkan/`. It implements the RHI interfaces (`IDevice`, `ICommandList`, `IBuffer`, `ITexture`, etc.) so that the rest of the renderer (RenderPipeline, PostProcessor, passes) can run without backend-specific code.

- **Rendering model**: Vulkan-style record-then-submit. The pipeline records into a command list (immediate or deferred), then the device submits it. Dynamic rendering (`vkCmdBeginRendering` / `vkCmdEndRendering`) is used instead of render passes for offscreen targets.
- **Shaders**: SPIR-V binaries (`.spv`) loaded from `assets/shaders/vulkan/...`; logical names (e.g. `passes/deferred/gbuffer`) are resolved to paths by the device.
- **Memory**: VMA (Vulkan Memory Allocator) for buffers and images; handles are stored as `void`* in headers to avoid pulling `vulkan.h` into the public RHI.

---

## 2. File Layout and Build

All Vulkan sources are compiled only when `OPENEYE_USE_VULKAN` is set and the Vulkan SDK is found. The backend is then exposed via `OPENEYE_VULKAN_AVAILABLE` and `CreateVulkanDevice()`.


| File                            | Role                                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **CreateDeviceVulkan.cpp / .h** | Factory: `CreateVulkanDevice(windowHandle, width, height)` → `std::unique_ptr<IDevice>`. Constructs `DeviceVK` and calls `Initialize()`.                                                                                                                                                                                                                                             |
| **DeviceVK.h / DeviceVK.cpp**   | Core device: instance, physical device, logical device, queues, command pool, descriptor pool, default sampler, pipeline cache, **legacy set 0**, **compute set 0**, default/compute pipeline layouts, bindless set layout, timeline semaphore, fullscreen quad buffers, pipeline hash cache. No swapchain/present here.                                                             |
| **DeviceVKPresentation.cpp**    | Presentation and blit: `Present()`, `OnWindowResized()`, `BlitToDefaultFramebuffer()`, swapchain render pass and framebuffers, ImGui descriptor pool and `GetImGuiVulkanInitInfo()`, `SetPostBlitCallback`, and stubs for readback/blit/depth blit.                                                                                                                                  |
| **CommandListVK.h / .cpp**      | Command list: records into a `VkCommandBuffer`. Begin/End, SetRenderTarget (dynamic rendering), ClearRenderTarget, viewport, barriers, BindPipeline (legacy set 0 + empty set 1, push constants), Bind*Buffer/BindTexture, SetUniform*/SetPushConstants, Draw*, Dispatch*, fullscreen quad, InsertFence. Tracks bound textures/buffers for `UpdateLegacySet0` / `UpdateComputeSet0`. |
| **VulkanBindingUtils.h / .cpp** | Descriptor/pipeline layout helpers: `CreateLegacySet0`, `CreateComputeSet0`, `CreateDefaultPipelineLayout`, `CreateComputePipelineLayout`, `UpdateLegacySet0`, `UpdateComputeSet0`. Maps command-list bindings to `VkWriteDescriptorSet`.                                                                                                                                            |
| **VulkanEnums.h**               | RHI → Vulkan mapping: buffer/image usage, format, resource state (stage/access/layout), vertex format, blend, depth compare, topology, index type. Used by pipelines, command list, barriers.                                                                                                                                                                                        |
| **PipelineVK.h / .cpp**         | Graphics and compute pipeline creation: `CreateVkGraphicsPipeline`, `CreateVkComputePipeline`, `CreateGraphicsPipelineVK`, `CreateGraphicsPipelineFromNameVK`, `CreateComputePipelineVK`. Builds `VkPipeline` with dynamic viewport/scissor and dynamic rendering.                                                                                                                   |
| **VulkanPipelineHash.h / .cpp** | `HashPipelineDesc(desc)` for graphics pipeline cache lookup (avoid duplicate `VkPipeline` creation).                                                                                                                                                                                                                                                                                 |
| **PipelineLayoutVK.h**          | Thin wrapper: `PipelineLayoutDesc` + `VkDescriptorSetLayout` + `VkPipelineLayout`. Used by custom layout/descriptor-set path.                                                                                                                                                                                                                                                        |
| **ShaderVK.h / .cpp**           | Shader module: `CreateShaderVK` (stub), `CreateShaderFromBinaryVK` (SPIR-V), `TryLoadShaderPairVK` (load .spv pair). Holds `VkShaderModule`.                                                                                                                                                                                                                                         |
| **BufferVK.h / .cpp**           | Buffer resource: VMA-backed `VkBuffer`. CreateBufferVK, DestroyBufferVK, Map/Unmap/Update.                                                                                                                                                                                                                                                                                           |
| **TextureVK.h / .cpp**          | Texture resource: VMA-backed `VkImage` + `VkImageView`. CreateTextureVK, DestroyTextureVK, views, wrapper. `GetNativeTexture()` returns `VkImageView` for ImGui/backends.                                                                                                                                                                                                            |
| **RenderTargetVK.h / .cpp**     | Render target: N color textures + optional depth, or shared depth, or texture-backed (cubemap face). No `VkFramebuffer` for dynamic rendering; provides `GetColorImageView`/`GetDepthImageView` for `vkCmdBeginRendering`. Create*VK / DestroyRenderTargetVK.                                                                                                                        |
| **DescriptorSetVK.h / .cpp**    | Descriptor set from a pipeline layout: UpdateTexture, UpdateUniformBuffer, UpdateStorageBuffer. Used when a pass uses explicit layout/sets rather than legacy set 0.                                                                                                                                                                                                                 |
| **BindlessHeapVK.h / .cpp**     | Bindless heap: one descriptor set with texture array (binding 0) and buffer array (binding 1). AllocateTextureHandle / AllocateBufferHandle, Free*. Used as set 1 when bound; otherwise empty bindless set is bound.                                                                                                                                                                 |
| **FenceVK.h / .cpp**            | Fence backed by device timeline semaphore. `InsertFence()` reserves a value; submit signals it; `WaitFence` waits until that value.                                                                                                                                                                                                                                                  |
| **QueryPoolVK.h / .cpp**        | Query pool: `VkQueryPool` for timestamps (or other types). Create/destroy, GetResult.                                                                                                                                                                                                                                                                                                |
| **vk_mem_alloc.h**              | Third-party VMA; included once in DeviceVK.cpp with `VMA_IMPLEMENTATION`.                                                                                                                                                                                                                                                                                                            |


---

## 3. Entry Point and Device Creation

1. **Application** requests Vulkan via `RenderBackend::Vulkan` and calls `platform::CreateGraphicsDevice(window, w, h, backend)` (see `CreateDevice.cpp`).
2. **CreateDevice.cpp** (platform) calls `rhi::CreateVulkanDevice(glfwWindow, width, height)` when Vulkan is available.
3. **CreateDeviceVulkan.cpp** builds `DeviceVK`, calls `device->Initialize(windowHandle, width, height)`.
4. **DeviceVK::Initialize** (in DeviceVK.cpp):
  - Creates `VkInstance` (with GLFW surface extension when needed).
  - Picks physical device and queue families (graphics, compute, transfer).
  - Creates `VkDevice` with required extensions (swapchain, etc.).
  - Creates `VmaAllocator`.
  - Creates command pool and **one** primary command buffer (immediate command list), assigns it via `m_immediateCommandList->SetCommandBuffer(immBuf, true)`.
  - Creates main descriptor pool, default sampler, pipeline cache.
  - Creates **legacy set 0** (layout + set) and **bindless set layout** + **empty bindless set** via VulkanBindingUtils.
  - Creates **default pipeline layout** (set 0 legacy + set 1 bindless + 128-byte fragment push constants).
  - Creates **compute set 0** (layout + set), **compute pipeline layout**, and **legacy compute params UBO** (256 bytes, binding 1).
  - Creates **timeline semaphore** for fences.
  - Creates **surface** and **swapchain** (and related state) — swapchain creation is in DeviceVK.cpp; resize and present/blit are in DeviceVKPresentation.cpp.

If any step fails, Initialize returns false and the factory returns nullptr.

---

## 4. Per-File Role (Detailed)

### 4.1 CreateDeviceVulkan

- **CreateVulkanDevice(windowHandle, width, height)**: Allocates `DeviceVK`, runs `Initialize(...)`. Returns the device or nullptr. Only public entry for the Vulkan backend.

### 4.2 DeviceVK (core)

- **Constructor**: Creates `CommandListVK` (no buffer yet), calls `QueryCapabilities()`.
- **Destructor**: Tears down fullscreen quad buffers, immediate command list, command pool, sampler, descriptor pool, pipeline cache, default/compute pipeline layouts, legacy/compute set layouts and sets, bindless layout and empty pool/set, timeline semaphore, swapchain framebuffers/render pass, ImGui descriptor pool, VMA allocator, device, surface, instance.
- **Initialize**: As in §3 (instance → device → allocator → command buffer → descriptors → layouts → swapchain).
- **CreateCommandList / DestroyCommandList**: Allocates a `VkCommandBuffer` from the pool, wraps it in a `CommandListVK(device, buf)` (non-immediate). On destroy, frees the buffer via `FreeCommandBuffer`.
- **GetImmediateCommandList**: Returns the single `CommandListVK` that was given the primary buffer in Initialize. All editor/scene recording uses this list.
- **SubmitCommandList**: If the list has a pending timeline value (InsertFence), includes it in submit; submits the list’s buffer to the graphics queue; clears pending fence. Deferred lists are submitted in order; immediate list is typically not submitted here but in Present (see §7).
- **CreateBuffer / DestroyBuffer**: Delegates to CreateBufferVK / DestroyBufferVK (VMA).
- **CreateTexture / DestroyTexture / CreateTextureView / CreateTextureBuffer**: Delegates to TextureVK helpers; wrapper path for external GL texture is no-op on Vulkan.
- **CreateShader / CreateShaderFromBinary / TryLoadShaderFromPaths / DestroyShader**: ShaderVK creation; path resolution uses `GetShaderBackendName()` (e.g. `"vulkan"`) and `.spv` extension.
- **CreateGraphicsPipeline / CreateGraphicsPipelineFromName / CreateGraphicsPipelineFromBinary / CreateComputePipeline / DestroyPipeline**: PipelineVK creation; FromName resolves shader paths (e.g. `passes/deferred/gbuffer` → vertex/fragment .spv), builds descriptor from ShaderCache/engine. Pipeline hash cache avoids duplicate VkPipelines for the same desc.
- **CreateRenderTarget** (and variants): Delegates to CreateRenderTargetVK, CreateDepthOnlyRenderTargetArrayVK, CreateRenderTargetWithSharedDepthVK, CreateRenderTargetFromTextureVK, CreateRenderTargetFromTextureWithDepthVK. DestroyRenderTarget → DestroyRenderTargetVK.
- **CreateQueryPool / DestroyQueryPool**: QueryPoolVK.
- **CreatePipelineLayout / DestroyPipelineLayout**: PipelineLayoutVK (custom layouts).
- **CreateDescriptorSet / DestroyDescriptorSet**: DescriptorSetVK.
- **CreateBindlessHeap / DestroyBindlessHeap**: BindlessHeapVK.
- **UpdateLegacySet0(cmd) / UpdateComputeSet0(cmd)**: Forward to VulkanBindingUtils; write descriptor set from command list’s current bound textures/buffers/UBOs.
- **GetDefaultPipelineLayout / GetLegacySet0 / GetComputeSet0 / GetComputePipelineLayout / GetDefaultSampler / GetFullscreenQuadVBO/IBO / EnsureFullscreenQuad**: Accessors and lazy creation of fullscreen quad.
- **ResolveShaderPath**: Maps logical name + stage to asset path under `shaders/vulkan/`.
- **GetCachedGraphicsPipeline / RegisterGraphicsPipeline**: Hash-based cache for VkPipeline reuse.

### 4.3 DeviceVKPresentation

- **Present()**: If the immediate list is recording, ends it. Submits the immediate command buffer to the graphics queue. Presents the current swapchain image. Waits for queue idle (simple sync).
- **OnWindowResized(width, height)**: Recreates swapchain, image views, framebuffers; clears and repopulates swapchain-related state.
- **Finish()**: `vkQueueWaitIdle`.
- **BlitToDefaultFramebuffer(src)**:
  - **src == nullptr** (editor path): Acquires next swapchain image. If the immediate list is not recording, calls `imm->Begin()` (this **resets** the command buffer). Transitions swapchain image to color attachment, begins the swapchain render pass (clear), invokes **post-blit callback** (e.g. ImGui), ends render pass. Does **not** submit; Present() will submit.
  - **src != nullptr**: Acquires image, transitions src image to transfer src, swapchain to transfer dst, copies image, then either (a) transitions to color attachment and runs post-blit callback (ImGui) in render pass, or (b) transitions to present. No submit in this function.
- **BlitToDefaultFramebufferLetterbox**: Forwards to `BlitToDefaultFramebuffer(src)` (letterbox not implemented).
- **EnsureSwapchainRenderPassAndFramebuffers**: Creates a simple render pass (load/store color) and one framebuffer per swapchain image for overlay/ImGui.
- **EnsureImGuiDescriptorPool**: Creates a descriptor pool for ImGui (combined image sampler).
- **SetPostBlitCallback**: Stores a `std::function<void(void*)>`; the void* is the current `VkCommandBuffer` when the callback is invoked during BlitToDefaultFramebuffer.
- **GetImGuiVulkanInitInfo**: Fills `ImGuiVulkanInitInfo` (instance, physical device, device, queue family, queue, pipeline cache, descriptor pool, render pass, etc.) for ImGui_ImplVulkan_Init.
- *Blit / BlitDepthOnly / ReadPixel / PBO APIs**: Stubs (no-op or return false).

### 4.4 CommandListVK

- **Begin**: Resets the command buffer, starts recording (`vkBeginCommandBuffer`). Sets `m_isRecording = true`.
- **End**: Ends dynamic rendering if active, then `vkEndCommandBuffer`; sets `m_isRecording = false`.
- **SetRenderTarget(rt)**: Ends any current dynamic rendering. Builds `VkRenderingAttachmentInfo` for each color (load/store, LOAD_OP_LOAD) and depth. Calls `vkCmdBeginRendering`; stores `m_currentRenderTarget`.
- **ClearRenderTarget(rt, clear)**: Only runs if `rt == m_currentRenderTarget`. Uses `vkCmdClearAttachments` for color and depth.
- **SetViewport / SetViewportIndexed**: Sets viewport and scissor (dynamic state).
- **BarrierTexture / BarrierBuffer**: Inserts pipeline barrier (resource state → Vulkan stage/access/layout via VulkanEnums).
- **BindPipeline(pipeline)**: Binds `VkPipeline`. For graphics: calls `UpdateLegacySet0(this)`, then binds descriptor sets 0 (legacy) and 1 (empty bindless) with `vkCmdBindDescriptorSets`. For compute: `UpdateComputeSet0`, binds compute set 0 and set 1. Push constants (128 bytes fragment or 16 bytes compute) are applied from `m_pushConstants` when SetUniformMat4/SetPushConstants have been used.
- **BindVertexBuffer / BindIndexBuffer**: Stores for the current draw; actual `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer` at Draw* time if different from last.
- **BindTexture / UnbindTexture**: Stores in `m_boundTextures[binding]`; used by UpdateLegacySet0.
- **BindUniformBuffer / BindStorageBuffer**: Stored for legacy set 0 or compute set 0 updates.
- **SetUniformMat4** (and other SetUniform*): Write into `m_pushConstants` at known offsets (e.g. inverse view/projection for skybox). Names like `u_InverseViewMatrix` map to offset 0, `u_InverseProjectionMatrix` to 64 (128-byte fragment push range).
- **SetPushConstants**: Direct copy into `m_pushConstants` and then `vkCmdPushConstants`.
- **DrawFullscreenQuad(pipeline)**: Binds pipeline if different, updates legacy set 0, binds sets 0 and 1, pushes 128 bytes, binds fullscreen quad VBO/IBO, draws. Used by post-process and skybox fullscreen passes.
- **Draw / DrawIndexed / DrawIndexedInstanced / Draw*Indirect / MultiDraw*Indirect / MultiDraw*IndirectCount**: Record draw calls; flush vertex/index bindings and push constants before draw.
- **Dispatch / DispatchIndirect**: Compute; UpdateComputeSet0, bind sets, push constants, then vkCmdDispatch.
- **InsertFence**: Reserves next timeline value, stores in command list; device signals it on next submit.
- **CopyBuffer**: Single `vkCmdCopyBuffer`.
- **EndRenderingIfActive**: If a render target is bound, calls `vkCmdEndRendering` and clears `m_currentRenderTarget`. Used before switching to a different pass or before the blit path uses the same command buffer.

### 4.5 VulkanBindingUtils

- **CreateLegacySet0**: Creates a descriptor set layout (image bindings 0,1,2,3,6,11,12,13; storage 5,7) and allocates one set from the pool. Used for graphics “set 0” that mirrors OpenGL-style bind slots.
- **CreateComputeSet0**: Layout + set for compute (storage 0,2,3; UBO 1,5; image 4).
- **CreateDefaultPipelineLayout**: Two set layouts (legacy + bindless) + one push constant range (fragment, 0, 128).
- **CreateComputePipelineLayout**: Compute set 0 + bindless set 1 + push 16 bytes.
- **UpdateLegacySet0(device, cmd)**: For each legacy binding, if the command list has a bound texture or storage buffer, writes a `VkWriteDescriptorSet` (combined image sampler or storage buffer). Uses device’s default sampler for images. Called from CommandListVK::BindPipeline (graphics) and before DrawFullscreenQuad.
- **UpdateComputeSet0(device, cmd)**: Copies compute params (256 bytes) into legacy compute params UBO; then writes descriptor set from command list’s bound storage buffers, UBOs, and image. Called from CommandListVK::BindPipeline (compute) and before Dispatch.

### 4.6 PipelineVK

- **CreateVkGraphicsPipeline**: Takes PipelineDesc, pipeline layout, cache. Builds vertex input from VertexLayout, fixed function state (viewport, raster, blend, depth/stencil, multisample), **dynamic state** (viewport, scissor), and **dynamic rendering** (no render pass). Returns `VkPipeline`.
- **CreateVkComputePipeline**: Single compute stage, layout, cache; returns `VkPipeline`.
- **CreateGraphicsPipelineFromNameVK**: Resolves shader name to vulkan paths (e.g. `passes/deferred/gbuffer` → `.vert.spv` / `.frag.spv`), loads SPIR-V, builds PipelineDesc, checks hash cache, creates pipeline, registers in cache.
- **CreateComputePipelineVK**: Builds compute pipeline from compute shader module and compute pipeline layout.

### 4.7 ShaderVK

- **CreateShaderFromBinaryVK**: Validates SPIR-V magic, creates `VkShaderModule`.
- **TryLoadShaderPairVK**: Reads two .spv files, creates vertex and fragment ShaderVK. Used by pipeline-from-name and engine shader loading.

### 4.8 BufferVK / TextureVK / RenderTargetVK

- **BufferVK**: VMA allocation; `m_buffer` (VkBuffer), `m_allocation` (VmaAllocation). Map/Unmap/Update for host-visible or staging.
- **TextureVK**: VMA image + image view; format and usage from descriptor. CreateTextureView creates another view (e.g. different format/layer); CreateTextureBuffer is stub. GetNativeTexture() = `m_imageView` for ImGui.
- **RenderTargetVK**: Holds N color textures and optional depth (or shared depth, or external texture for cubemap). GetColorImageView(i) / GetDepthImageView() return views for dynamic rendering. Depth array layer supported for shadow cascades.

### 4.9 DescriptorSetVK / BindlessHeapVK

- **DescriptorSetVK**: Per custom pipeline layout; UpdateTexture/UpdateUniformBuffer/UpdateStorageBuffer write into the set. Used when a pass uses explicit descriptor sets instead of legacy set 0.
- **BindlessHeapVK**: One set with large texture and buffer arrays; AllocateTextureHandle/AllocateBufferHandle return handles used in bindless shaders. Set 1 is either this heap’s set or the device’s empty bindless set.

### 4.10 FenceVK / QueryPoolVK

- **FenceVK**: Holds timeline value; WaitFence waits on the device timeline semaphore until value is reached.
- **QueryPoolVK**: VkQueryPool for timestamps (or other types); GetResult for CPU readback (after sync).

---

## 5. Descriptor and Binding Model

- **Graphics (default)**  
  - **Set 0**: “Legacy” set — bindings 0,1,2,3,6,11,12,13 = combined image sampler; 5,7 = storage buffer. Filled from `CommandListVK`’s `BindTexture` / `BindStorageBuffer` via `UpdateLegacySet0`.  
  - **Set 1**: Bindless — either the bound `BindlessHeapVK`’s set or the device’s **empty bindless set** (so set 1 is always bound).  
  - **Push constants**: One range, fragment, offset 0, size 128 bytes (e.g. inverse view/projection, flags). Written by SetUniformMat4/SetPushConstants.
- **Compute**  
  - **Set 0**: Storage 0,2,3; UBO 1 (params), 5; image 4. Filled from command list + legacy compute params UBO via `UpdateComputeSet0`.  
  - **Set 1**: Same bindless set as graphics.  
  - **Push constants**: 16 bytes.

Custom pipelines can use `IPipelineLayout` / `IDescriptorSet` (PipelineLayoutVK, DescriptorSetVK) instead of the default layout and legacy set.

---

## 6. Command Buffer and Submission Flow

1. **Frame**: No explicit “BeginFrame” in the editor. The **scene view pipeline** calls `GetRHICommandList()` (immediate list), then `cmd->Begin()`, records all passes (GBuffer, shadows, lighting, skybox, post, etc.), then `cmd->End()`.
2. **Editor (Vulkan)** then calls `BlitToDefaultFramebuffer(nullptr)`. Because the list was ended, `IsRecording()` is false, so the blit path calls `imm->Begin()`, which **resets** the command buffer and starts a new recording. Only the swapchain clear and post-blit callback (ImGui) are then recorded; **all scene commands are discarded**.
3. **Present()** ends the list if still recording, submits the (now blit-only) buffer, presents, and waits for queue idle.

So in the **current** editor path, the scene is never submitted; only the blit + ImGui pass is. This is why the scene panel can appear black. The fix is to keep the same command buffer open (pipeline does not call `End()`) so that the blit path only appends, and to have Present() end and submit once. See conversation summary / vulkan_plan for the intended fix.

---

## 7. Swapchain and Presentation

- **Swapchain** is created in DeviceVK::Initialize (and recreated in OnWindowResized in DeviceVKPresentation).
- **BlitToDefaultFramebuffer(nullptr)** acquires the next image, (currently) resets the command buffer and records clear + post-blit callback into the same buffer.
- **Present()** submits that buffer and presents. No separate “scene” and “blit” command buffers in the current design; both are intended to be in the same buffer when the pipeline does not call `End()` before blit.

ImGui Vulkan backend is initialized with `GetImGuiVulkanInitInfo()`. The editor passes the pipeline output texture’s native handle (VkImageView) to ImGui; it is resolved to a VkDescriptorSet via `ImGui_ImplVulkan_AddTexture` in the editor’s `resolveImGuiTextureId` so ImGui can sample it.

---

## 8. Build Integration

- **CMake** (eyelib/render/CMakeLists.txt): When `OPENEYE_USE_VULKAN` and `Vulkan_FOUND`, adds all Vulkan .cpp files to RENDER_SOURCES and sets `OPENEYE_VULKAN_AVAILABLE`.
- **CreateDevice.cpp** (rhi) does not reference Vulkan; **platform** CreateGraphicsDevice (in same file, different namespace) calls `CreateVulkanDevice` when `RenderBackend::Vulkan` is requested and `OPENEYE_VULKAN_AVAILABLE` is set.

---

## 9. Known Gaps and Stubs

- **Blit / BlitDepthOnly**: Not implemented (stubs). Depth copy from GBuffer to HDR is not done on Vulkan.
- **ReadPixelUint32 / ReadPixelsRGBA8 / PBO**: Not implemented; return false or no-op.
- **CreateTextureBuffer**: Returns nullptr.
- **CommitSparseTexturePages / CommitSparseBufferPages**: No-op.
- **Editor scene view black**: Caused by command buffer reset in `BlitToDefaultFramebuffer(nullptr)` discarding scene commands; pipeline must not call `End()` before blit so both scene and blit live in one buffer (see §6 and vulkan_plan / conversation summary).
- **Barrier for pipeline output**: Before ImGui samples the scene output texture, it should be transitioned to `SHADER_READ_ONLY_OPTIMAL` (e.g. BarrierTextureForShaderRead on the current frame output texture before ending the pipeline’s recording or before the blit phase).

---

## 10. Dependency Graph (Conceptual)

```
CreateDeviceVulkan  →  DeviceVK (Initialize)
DeviceVK            →  CommandListVK, VulkanBindingUtils, PipelineVK, ShaderVK,
                       BufferVK, TextureVK, RenderTargetVK, DescriptorSetVK,
                       BindlessHeapVK, FenceVK, QueryPoolVK, VulkanEnums, VulkanPipelineHash
DeviceVK            →  DeviceVKPresentation (same class, different .cpp: Present, Blit, resize, ImGui)
CommandListVK       →  DeviceVK (UpdateLegacySet0, UpdateComputeSet0, fullscreen quad, pipeline cache)
PipelineVK          →  ShaderVK, VulkanEnums, VulkanPipelineHash, DeviceVK
VulkanBindingUtils  →  DeviceVK, CommandListVK, BufferVK, TextureVK
RenderTargetVK       →  TextureVK
```

The report above reflects the **current** state of the Vulkan backend; for a forward-looking roadmap, see `vulkan_plan.md`.