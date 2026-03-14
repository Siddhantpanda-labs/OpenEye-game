Below is a **practical implementation roadmap** to build a **modern, powerful Vulkan backend** under your existing RHI. It’s ordered so each stage builds on the previous one and keeps your renderer stable while gaining performance and features.

I’ll reference Unreal Engine and Vulkan as the target level of capability.

---

# 0. Guiding Principles (before coding)

Design the Vulkan backend around these rules:

1. **Explicit ownership** – no hidden state.
2. **GPU-driven workflow** – minimize CPU decisions.
3. **Frame graph integration** – passes own resource states.
4. **Bindless / descriptor indexing** – avoid descriptor churn.
5. **Async compute capable** – separate queues.
6. **Multithread command recording**.

Your current RHI already fits this model well.

---

# 1. Backend Skeleton

Create a Vulkan backend directory:

```
rhi/Vulkan/
    DeviceVK
    CommandListVK
    BufferVK
    TextureVK
    ShaderVK
    PipelineVK
    DescriptorSetVK
    PipelineLayoutVK
    RenderTargetVK
    QueryPoolVK
    FenceVK
```

Each implements the RHI interfaces.

Example:

```cpp
class DeviceVK : public IDevice
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;
};
```

---

# 2. Vulkan Instance + Device

Initialize Vulkan:

```
Create VkInstance
Select VkPhysicalDevice
Create VkDevice
Create queues
```

Enable extensions:

```
VK_KHR_swapchain
VK_EXT_descriptor_indexing
VK_KHR_buffer_device_address
VK_KHR_dynamic_rendering
VK_KHR_timeline_semaphore
VK_EXT_mesh_shader (optional)
VK_KHR_ray_tracing_pipeline (future)
```

These give **modern GPU features**.

---

# 3. Memory System

Do not allocate raw Vulkan memory everywhere.

Implement **memory allocator layer**.

Best approach:

```
Use Vulkan Memory Allocator (VMA)
```

Resource classes:

```
BufferVK
TextureVK
```

Example:

```cpp
VmaAllocation allocation;
VkBuffer buffer;
```

Memory types:

```
GPU_ONLY
CPU_TO_GPU
GPU_TO_CPU
```

---

# 4. Command System

Your RHI already has:

```
Immediate command list
Deferred command lists
```

Map them to Vulkan:

```
VkCommandPool
VkCommandBuffer
```

Implementation:

```
CommandListVK
```

Multithread recording:

```
thread → own command pool
```

Submission:

```
Submit to queue
Signal fence
```

---

# 5. Resource State Tracking

Your RHI already has:

```
ResourceState
Barriers
```

Map them to Vulkan:

```
VkPipelineStageFlags
VkAccessFlags
VkImageLayout
```

Implement a **resource state tracker**.

Example transition:

```
COLOR_ATTACHMENT → SHADER_READ
```

Convert to:

```
VkImageMemoryBarrier
```

---

# 6. Descriptor System

AAA Vulkan backends rely heavily on descriptor management.

Implement:

```
DescriptorAllocatorVK
DescriptorSetVK
PipelineLayoutVK
```

Use:

```
VK_EXT_descriptor_indexing
```

Features:

```
bindless textures
bindless buffers
```

Example bindless heap:

```
descriptor array size = 100000
```

Shader:

```
texture2D textures[];
```

---

# 7. Pipeline System

Pipeline objects are expensive.

Implement a **pipeline cache system**.

```
PipelineVK
```

Structure:

```
PipelineDesc → hash
hash → VkPipeline
```

Store pipelines in:

```
PipelineCache
```

Use:

```
VkPipelineCache
```

---

# 8. Shader System

Compile shaders with:

```
GLSL → SPIR-V
```

Tools:

```
glslangValidator
DXC
```

Load SPIR-V:

```
VkShaderModule
```

Add **reflection system**:

Extract:

```
descriptor sets
push constants
bindings
```

Libraries:

```
SPIRV-Reflect
```

---

# 9. Dynamic Rendering

Avoid traditional render passes.

Use:

```
VK_KHR_dynamic_rendering
```

Then rendering becomes:

```
vkCmdBeginRendering
vkCmdEndRendering
```

This matches your **RHI render target abstraction** better.

---

# 10. Bindless Resource System

Implement bindless heap mapping to Vulkan descriptor arrays.

Your RHI already has:

```
IBindlessHeap
```

Implementation:

```
Descriptor set with huge arrays
```

Example:

```
textures[65536]
buffers[65536]
```

Material stores indices.

---

# 11. Frame Graph Integration

Frame graph handles:

```
resource lifetimes
barriers
pass order
```

Backend must provide:

```
efficient barriers
transient attachments
```

Transient resources:

```
VkImage with lazily allocated memory
```

---

# 12. GPU Synchronization

Avoid traditional fences.

Use:

```
timeline semaphores
```

Example:

```
frameCounter++
signal timeline semaphore
wait value
```

This simplifies GPU scheduling.

---

# 12a. Scene render-capable (minimum for on-screen)

To actually render the scene and show it in the window, the backend must complete these before or alongside the advanced steps:

**1. Swapchain and present**

- Create `VkSwapchainKHR` at init (or resize) using the window surface and desired format.
- In the frame loop: acquire next image (`vkAcquireNextImageKHR`), optionally sync with a semaphore.
- `Present()` must: submit work that uses the current swapchain image (or a blit into it), then call `vkQueuePresentKHR` with the swapchain and image index. Replace the current stub that only submits and waits idle.
- Handle swapchain recreate on resize or `VK_SUBOPTIMAL_KHR` / `VK_ERROR_OUT_OF_DATE_KHR`.

**2. Draw and bind commands**

- `BindVertexBuffer`: call `vkCmdBindVertexBuffers` with the buffer and offset (and current binding slot).
- `BindIndexBuffer`: call `vkCmdBindIndexBuffer` with buffer, offset, and index type (`VK_INDEX_TYPE_UINT16` / `UINT32`).
- `Draw`: call `vkCmdDraw(vertexCount, instanceCount, firstVertex)` (instanceCount = 1 for non-instanced).
- `DrawIndexed`: call `vkCmdDrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)` (instanceCount/firstInstance = 1, 0 if not used).
- Optionally implement `DrawFullscreenQuad` (bind quad VB/IB if needed, then draw) so fullscreen passes work.

**3. Path from render target to window**

- Either: render directly into the acquired swapchain image (SetRenderTarget to a RT built from that image, or add an internal “default framebuffer” RT for the current swapchain image), or
- Implement `BlitToDefaultFramebuffer(IRenderTarget* src)` so the main scene RT can be blitted/copied to the current swapchain image before present.

After these, the Vulkan backend is **scene render-capable**: it can record draws, render to a target or swapchain, and present to the window.

---

# 13. GPU Driven Rendering

Implement GPU scene buffers.

```
GPUScene
```

Structure:

```
instance data
mesh data
material data
```

Rendering uses:

```
vkCmdDrawIndexedIndirectCount
```

This allows **millions of objects**.

---

# 14. Cluster / Meshlet Geometry

To reach **Nanite-style scalability**, implement mesh clusters.

Mesh preprocessing:

```
split mesh into clusters (~128 triangles)
```

Cluster buffer:

```
cluster bounding box
cluster triangle data
```

GPU pipeline:

```
compute culling
indirect draw
```

---

# 15. Async Compute

Use compute queue for:

```
cluster light culling
SSR
post processing
occlusion
```

While graphics queue renders geometry.

---

# 16. GPU Culling

Compute shaders perform:

```
frustum culling
occlusion culling
LOD selection
```

Output:

```
indirect draw commands
```

Renderer consumes them directly.

---

# 17. Mesh Shader Pipeline (optional)

If supported:

```
VK_EXT_mesh_shader
```

Pipeline:

```
task shader
mesh shader
fragment shader
```

Allows:

```
cluster rendering
LOD
GPU culling
```

---

# 18. Advanced Lighting

Your Forward+ system integrates easily.

Enhance with:

```
clustered lighting
GPU light culling
light list compaction
```

Allow:

```
1000+ lights
```

---

# 19. Ray Tracing (future)

Add optional ray tracing pipeline:

```
VK_KHR_ray_tracing_pipeline
```

Uses:

```
acceleration structures
ray tracing shaders
```

For:

```
GI
reflections
shadows
```

---

# 20. Render Graph Optimizations

Frame graph improvements:

```
pass merging
async scheduling
resource aliasing
```

Reduces memory and barriers.

---

# 21. Debug + Profiling

Integrate:

```
VK_EXT_debug_utils
```

GPU profiling:

```
timestamp queries
pipeline statistics
```

---

# Final Architecture

Your renderer becomes:

```
Renderer
    ↓
FrameGraph
    ↓
RHI
    ↓
Vulkan Backend
        DeviceVK
        CommandListVK
        PipelineVK
        DescriptorVK
        BufferVK
        TextureVK
        SyncVK
```

GPU pipeline:

```
Compute (culling)
↓
Graphics (draw)
↓
Post processing
```

---

# Result

With this architecture you achieve:

```
GPU driven rendering
bindless resource system
multithread command recording
async compute
clustered lighting
indirect rendering
mesh cluster scalability
```

Which is the **modern architecture used by large engines**.

---

# Progress tracker

| # | Stage | Status |
|---|--------|--------|
| 1 | Backend skeleton (DeviceVK, CommandListVK, BufferVK, TextureVK, ShaderVK, PipelineVK, DescriptorSetVK, PipelineLayoutVK, RenderTargetVK, QueryPoolVK, FenceVK) | Done |
| 2 | Instance + device + queues | Done |
| 3 | Memory (VMA) | Done |
| 4 | Command system (pools, buffers, submit) | Done |
| 5 | Resource state tracking / barriers | Done |
| 6 | Descriptor system | Done |
| 7 | Pipeline cache | Done |
| 8 | Shaders (SPIR-V + reflection) | Done |
| 9 | Dynamic rendering | Done |
| 10 | Bindless | Done |
| 11 | Frame graph integration | Done |
| 12 | Timeline semaphores | Done |
| **12a.1** | **Swapchain (create, acquire, present in Present())** | Done |
| **12a.2** | **Draw path (BindVertexBuffer, BindIndexBuffer, Draw, DrawIndexed)** | Done |
| **12a.3** | **Path to window (render to swapchain or BlitToDefaultFramebuffer)** | Done |
| **12b** | **Pipeline and shaders** | Done |
| 13+ | GPU scene, async compute, mesh/ray tracing, etc. | Pending |

**12b. Pipeline and shaders (deferred/forward+):**
- **Done:** (1) `CreateGraphicsPipelineFromName` loads SPIR-V from `assets/shaders/vulkan/{name}.vert.spv` / `.frag.spv`. (2) **Lighting pass:** Vulkan `lighting.vert`/`.frag` use `layout(push_constant) uniform LightingPush { int u_UseIBL; layout(offset=16) mat4 u_ViewProj; }`. Default pipeline layout has push constant range (fragment, 0, 128). `CommandListVK::SetUniformInt("u_UseIBL", ...)` / `SetUniformMat4("u_ViewProj", ...)` write into `m_pushConstants`; `DrawFullscreenQuad` calls `vkCmdPushConstants` then draws. (3) **Legacy set 0:** Descriptor set layout with bindings 0,1,2,3,6,11,12,13 = combined image sampler, 5,7 = storage buffer. `BindTexture` / `BindStorageBuffer` store in command list; `UpdateLegacySet0(cmd)` updates the set from cmd state; `DrawFullscreenQuad` calls it and binds set 0 before drawing. (4) **DrawFullscreenQuad** implemented: ensure fullscreen quad, bind VBO/IBO, update and bind set 0, push constants, `vkCmdDrawIndexed(6)`. (5) Vulkan shaders: `passes/deferred/lighting`, `passes/post/copy` (GLSL + CMake glslc → .spv); CopyEngineAssets depends on VulkanShaders.
- **Optional next:** Material-compiled shaders (gbuffer, shadow, forward, forward+) need runtime GLSL→SPIR-V in `DeviceVK::CreateShader` (or prebuilt .spv per variant) for full deferred/forward+ scene on Vulkan. Add more pass shaders (tonemap, skybox, etc.) as needed.

---

### Descriptor binding: compatibility way vs pure Vulkan way

**Compatibility way (current)**

The RHI was designed around an OpenGL-style “bind by slot” model: call `BindTexture(cmd, texture, 0)`, `BindStorageBuffer(cmd, buffer, 5, offset, size)`, etc. The driver (or our code) conceptually keeps “whatever is currently bound at slot N.” Vulkan has no such global slots; it uses **descriptor sets** with a fixed layout per set.

The **compatibility approach** keeps the existing RHI API unchanged:

- **One fixed descriptor set (set 0)** is created with a layout that matches the binding numbers used by current shaders (e.g. 0,1,2,3,6,11,12,13 = combined image sampler, 5,7 = storage buffer). This is the “legacy” set 0.
- The **command list** stores the current bindings in arrays (`m_boundTextures[]`, `m_boundStorageBuffers[]`, etc.), like GL would track state.
- Before a draw that uses these bindings, the backend **flushes** that state into the single descriptor set: `UpdateLegacySet0(cmd)` builds `VkWriteDescriptorSet` entries from the command list’s arrays and calls `vkUpdateDescriptorSets`. Then it binds set 0 and draws.

**Pros:** No changes to callers (LightingPass, GBuffer, etc.); they keep using `BindTexture` / `BindStorageBuffer` by binding index. One layout and one set cover all pipelines that use this binding pattern. **Cons:** The layout is a superset of bindings that might be used by any such pipeline; we update the set every time before a draw (even when bindings haven’t changed). Name-based uniforms (e.g. `SetUniformMat4("u_ViewProj", ...)`) are handled separately (e.g. push constants) and only for known names.

**Pure Vulkan way (alternative design)**

A Vulkan-native design would avoid a single global “slot” set and instead:

- **Per-pipeline descriptor set layout:** When creating a graphics pipeline (e.g. from SPIR-V), reflect the shader to get its descriptor set layout(s) and create a `VkPipelineLayout` that uses those layouts. Each pipeline then has a layout that matches exactly what its shaders use.
- **Explicit descriptor sets per pass:** Each pass (lighting, gbuffer, etc.) would allocate or reuse descriptor sets from the appropriate layout, fill them with the resources that pass needs (e.g. GBuffer textures, shadow map, scene lighting buffer), and call `BindDescriptorSet(cmd, set)`. The RHI could expose something like “create a descriptor set for this pipeline layout” and “update this descriptor set with these bindings.”
- **No “current slot” state in the command list:** Instead of `BindTexture(cmd, tex, 0)` updating internal arrays, the caller would build a descriptor set (or use a prebuilt one) and bind that set before drawing. Push constants or a small UBO could still be used for per-draw data (view-proj, flags, etc.).

**Pros:** Aligns with Vulkan’s model; only the bindings actually used by a pipeline are in its layout; descriptor set updates can be cached or batched per pass. **Cons:** Requires RHI or pass-level changes: either the RHI gains a “descriptor set builder” or Vulkan-specific APIs, or each pass is updated to create/update descriptor sets explicitly instead of calling `BindTexture`/`BindStorageBuffer` by slot.

**Summary**

| Aspect | Compatibility (current) | Pure Vulkan |
|--------|---------------------------|-------------|
| API | Existing `BindTexture` / `BindStorageBuffer` by binding index | Explicit descriptor sets (new or extended API) |
| Set 0 | One fixed “legacy” layout and set; updated from cmd state before draw | Per-pipeline layout; pass creates/updates sets as needed |
| Call sites | Unchanged | Passes (or RHI layer) create and bind sets explicitly |

The current implementation uses the **compatibility way** so that the Vulkan backend works with the existing RHI and passes without refactoring. A future refactor could introduce the **pure Vulkan way** (e.g. descriptor set APIs and per-pipeline layouts) if we want to fully align with Vulkan’s binding model and reduce redundant descriptor updates.

---
