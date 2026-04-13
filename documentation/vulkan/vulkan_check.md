# Vulkan backend — change log & discipline tracker

**Purpose:** Companion to [`Vulkan_Backend_Architecture.md`](Vulkan_Backend_Architecture.md) for **ongoing** Vulkan changes after the bulk hardening pass.

**Implemented summary (2026-04-12, phases 0–11):** Header **contracts** across `eyelib/rendering/rhi/Vulkan/`; **logging** on many failure-prone paths (`VkResult`, pools, swapchain acquire or present, shader compile, etc.); **`VK_EXT_debug_utils`** naming and optional **`CommandListVK`** debug labels; **`DeviceVK`** threading and lifetime. **One-page table:** **Phase roadmap — implemented work** in the architecture doc.

**Rules (future rows)**

- One row per **meaningful** follow-up change (not every typo).
- Tie to a **topic** (phase 0–11 or `misc`) when helpful.
- Note **validation** when relevant.

---

## Change log

| Date | Topic | Summary |
|------|-------|---------|
| 2026-04-12 | 0–11 | Bulk hardening: documentation contracts, validation-oriented logging, debug utils, targeted fixes—see architecture doc **Phase roadmap — implemented work** table. |
| 2026-04-13 | misc | **RHICapabilities:** `supportsPresentMailbox` / `supportsPresentImmediate` (surface present-mode query), `supportsGpuTimestampQueries` (`timestampPeriod`); **tessellation / geometry** caps mirror `VkPhysicalDeviceFeatures` after `vkGetPhysicalDeviceFeatures2`; **`RenderBackendPolicy`** helpers for present + timestamps; **`CreateQueryPoolVK`** gates on `supportsGpuTimestampQueries`. |
| 2026-04-13 | misc | **Occlusion queries:** `QueryType::Occlusion`, `RHICapabilities::supportsOcclusionQueries` (Vulkan path: baseline **`VK_QUERY_TYPE_OCCLUSION`**; legacy feature bit absent in Vulkan 1.4+ headers); **`CreateQueryPoolVK`** + **`GetResult`** (raw sample count); **`ICommandList::ResetQueryPool`** → `vkCmdResetQueryPool` (**outside** active dynamic rendering, same rule as timestamp reset); **`BeginQuery`/`EndQuery`** branch timestamps vs `vkCmdBeginQuery`/`vkCmdEndQuery`; **`RenderBackendPolicy::SupportsOcclusionQueries`**. |
| 2026-04-13 | misc | **Host query reset:** `VkPhysicalDeviceHostQueryResetFeatures` chained in **`DeviceVK::Initialize`**; **`RHICapabilities::supportsHostQueryReset`**; **`IDevice::ResetQueryPoolOnHost`** → `vkResetQueryPool`; **`RenderBackendPolicy::SupportsHostQueryReset`**. **`BeginQuery`(Timestamp)** and **`CommandListVK::ResetQueryPool`** use host reset when the cap is on so resets work **inside** dynamic rendering (callers must still satisfy Vulkan **unavailable** query state before reset). |

---

## Notes

- **Deprecated:** `eyelib/render/rhi/Vulkan` — do not track here; use **`eyelib/rendering/rhi/Vulkan`** only.
- **Ongoing:** GPU validation on ship targets, optional CI smoke tests—manual per release.

---

## How to update

1. Append a **new row** to **Change log** for subsequent Vulkan-focused work.
2. Add a matching line under **Progress log** in `Vulkan_Backend_Architecture.md` if it is a milestone.
3. Cross-reference `documentation/Render_Module/Render_check.md` if public RHI or harness behavior changes.

---

## Capability matrix (vs [`Must_have_stuff.md`](Must_have_stuff.md))

**Scope:** `EngineSource/eyelib/rendering/rhi` (Vulkan backend under `Vulkan/`). This folder exposes a Vulkan device factory (`CreateVulkanDevice`); there is no second backend implementation here—RHI types are backend-agnostic, but **only the Vulkan path is audited**.

**Status vocabulary**

| Status | Meaning |
|--------|---------|
| **Yes** | Present and used on the Vulkan path with real behavior. |
| **Partial** | Implemented in part, optional/compile-time, or infrastructure without full feature coverage. |
| **Stub** | RHI surface or `RHICapabilities` may claim support, but the Vulkan implementation is empty or non-functional. |
| **No** | Not implemented in this tree. |

**Snapshot:** 2026-04-13 — refresh when code changes.

### RHI layer — core responsibilities

| Topic | Status | Notes / location |
|-------|--------|------------------|
| Opaque handle-based API (no Vulkan types in public headers) | **Partial** | `RHI.h` / `RHITypes.h` use interfaces; some ImGui init uses `void*` for interop. |
| Multi-backend dispatch (Vulkan / D3D12 / Metal / WebGPU) | **No** | Only `Vulkan/core/CreateDeviceVulkan.cpp` in this tree. |
| Typed handles + free-list slot maps | **No** | Interface pointers; no central slot-map layer in `rhi/`. |
| Explicit lifetime + deferred destruction | **Yes** | `memory/DeletionQueueVK.h`, frame contexts, device teardown patterns in `DeviceVK`. |
| Command list begin/end, pools | **Yes** | `commands/CommandPoolVK.*`, `CommandListVK.*`, `FrameContextVK.*`. |
| Render pass abstraction (attachments, load/store) | **Partial** | Dynamic rendering path (`vkCmdBeginRendering` / `EndRendering` in `CommandListVK.cpp`); not classic `VkRenderPass` graph. |
| Compute + copy | **Yes** | `vkCmdDispatch` + **`DispatchIndirect`** (`CommandListVK.cpp`; same compute bind path as `Dispatch`). |
| Indirect draw / multi-draw / indirect dispatch | **Yes** | `vkCmdDrawIndirect` / `DrawIndexedIndirect` / multi-draw variants / `vkCmdDispatchIndirect` in `CommandListVK.cpp`. **`supportsMultiDrawIndirectCount`** tracks Vulkan 1.2 `drawIndirectCount` (`DeviceVK::m_supportsDrawIndirectCount`). Callers must **`BarrierBuffer(..., IndirectDraw)`** before GPU reads indirect args. |
| Per-frame command allocator contention avoided | **Partial** | Frame contexts / pools; exact policy is implementation-specific. |
| Buffers, textures, samplers, RTVs | **Yes** | `resources/*VK`, `RenderTargetVK`, `TextureVK`, `BufferVK`. |
| SRV / UAV / RTV / DSV views | **Partial** | Model exists; verify per usage in pipeline binding. |
| Texture formats (BC, ASTC, sRGB, etc.) | **Partial** | Enum + creation paths; not every format combination audited here. |
| Mips, cubemap, arrays | **Partial** | Supported where `TextureDesc` / creation allows. |
| Graphics PSO | **Yes** | `pipelines/PipelineVK.cpp`, layout + shader modules. |
| Compute PSO | **Yes** | `CreateVkComputePipeline` in `PipelineVK.cpp`. |
| Mesh shader PSO | **No** | No `VK_EXT_mesh_shader` usage in `rhi`. |
| Tessellation / geometry shader (caps) | **Yes** | `supportsTessellation` / `supportsGeometryShader` set from **`features2.features`** after `vkGetPhysicalDeviceFeatures2` (`DeviceVK::Initialize`); **`QueryCapabilities`** copies from `m_supportsTessellationShader` / `m_supportsGeometryShader`. |
| Ray tracing PSO | **No** | `m_caps.supportsRayTracing = false` in `DeviceVK.cpp`. |
| PSO caching + disk serialization | **Yes** | `VkPipelineCache` + load/save in `DeviceVK.cpp` (`GetPipelineCachePathForPhysicalDevice`, `vkGetPipelineCacheData`). |
| SPIR-V / shader compile / reflection | **Partial** | `VulkanGlslToSpirv`, `ShaderVK`; reflection depth varies. |
| Offline pipeline + hot-reload | **Partial** | Asset/shader loading exists; hot-reload is product-level. |
| Shader permutations / variants | **Partial** | Hashing in `VulkanPipelineHash`; full permutation system not fully described here. |
| Root signature / push constants / descriptor layouts | **Yes** | `PipelineLayoutVK`, `VulkanResourceSpaces`, `vkCmdPushConstants`. |
| Bindless (descriptor indexing, heaps) | **Partial** | `BindlessHeapVK`, descriptor indexing + update-after-bind in `DeviceVK.cpp`; not descriptor buffers. |
| Persistent descriptor tables / heaps | **Partial** | `BindlessHeapVK`, `DescriptorAllocatorVK`, caches. |
| Dynamic UBOs + push constants | **Yes** | Push constants wired; dynamic offsets per RHI usage. |
| Samplers + immutable samplers | **Partial** | Samplers supported; immutable sampler detail in layout paths. |
| Render graph / auto barriers | **Partial** | No full frame graph in RHI; `barriers/BarrierTrackerVK.*` + `ResourceStateVK.h` assist tracking. |
| Fences / semaphores (CPU–GPU, GPU–GPU) | **Partial** | Timeline semaphore for device signaling (`DeviceVK.cpp`); `FenceVK` uses timeline wait path. |
| Resource state tracking | **Partial** | Barrier tracker + explicit barriers in command recording. |
| Platform swap chain | **Yes** | `SwapchainVK.cpp`, `DeviceVKPresentation.cpp` (Win32 surface path). |
| Present modes (mailbox / immediate / FIFO) | **Yes** | `SwapchainVK` maps `SwapchainPresentMode` (FIFO / Mailbox / Immediate) to a supported `VkPresentModeKHR`; `IDevice::SetSwapchainPresentMode` / `GetSwapchainPresentMode`. Mailbox requests ≥3 images when clamped by surface caps. **Discovery:** `RHICapabilities::supportsPresentMailbox` / `supportsPresentImmediate`; `eyelib::rendering::backend::SupportsPresentMailbox` / `SupportsPresentImmediate`. |
| GPU timestamp queries (caps) | **Yes** | `RHICapabilities::supportsGpuTimestampQueries` from `VkPhysicalDeviceLimits::timestampPeriod` (`DeviceVK::QueryCapabilities`); `SupportsGpuTimestampQueries` in `RenderBackendPolicy`; `CreateQueryPoolVK` rejects when false. |
| Occlusion queries (caps) | **Yes** | `RHICapabilities::supportsOcclusionQueries` set on the Vulkan path for **`VK_QUERY_TYPE_OCCLUSION`** (Vulkan 1.4+ headers no longer expose the legacy `occlusionQuery` feature bit in `VkPhysicalDeviceFeatures`). `SupportsOcclusionQueries` in `RenderBackendPolicy`; `CreateQueryPoolVK(QueryType::Occlusion)` rejects when false. |
| Host query reset (caps) | **Partial** | `RHICapabilities::supportsHostQueryReset` from `VkPhysicalDeviceHostQueryResetFeatures` (`DeviceVK::Initialize`). `IDevice::ResetQueryPoolOnHost`, `SupportsHostQueryReset` in `RenderBackendPolicy`. Optional on hardware; when off, cmd `vkCmdResetQueryPool` path still cannot run inside dynamic rendering. |
| HDR output (HDR10 / scRGB) | **No** | Not implemented in this RHI layer audit. |

### Vulkan backend — core concepts

| Topic | Status | Notes / location |
|-------|--------|------------------|
| Instance + validation layers + `VK_EXT_debug_utils` | **Partial** | Validation + debug utils when `OPENEYE_VULKAN_VALIDATION` is enabled (`InstanceVK.cpp`). |
| Physical device selection + feature queries | **Yes** | `PhysicalDeviceVK.cpp`. |
| Multiple queue families (graphics / compute / transfer) | **Partial** | Queues created in `QueuesVK`; **submits use the graphics queue** in reviewed paths—async compute overlap not exercised. |
| Device features (1.3 + extensions) | **Partial** | Dynamic rendering, descriptor indexing, timeline semaphores, optional `VK_EXT_extended_dynamic_state`; narrow extension set in `DeviceVK.cpp`. **Core optional stages:** tessellation / geometry **caps** track physical-device feature bits (not hard-coded). |
| Custom allocator (VMA-style) | **Yes** | `memory/AllocatorVK.*`, `vk_mem_alloc.h`. |
| Memory types / upload heaps / staging | **Yes** | `UploadHeapVK`, allocator integration. |
| Heap aliasing for transient RTs | **Partial** | Depends on allocation policy; not a dedicated sub-allocator feature row here. |
| Buffer device address (BDA) for GPU pointers | **No** | Not enabled/exposed in `DeviceVK` feature chain reviewed; VMA headers mention BDA only generically. |
| Sparse / partially resident resources | **Stub** | `CommitSparse*` on device; caps `supportsSparse*` false; commits are no-ops. |
| `VkRenderPass2` + subpasses | **No** | Dynamic rendering path instead of classic render passes in `CommandListVK`. |
| Dynamic rendering (`VK_KHR_dynamic_rendering` / 1.3) | **Yes** | Required at init; `vkCmdBeginRendering` in `CommandListVK.cpp`. |
| Imageless framebuffer | **No** | Not surfaced. |
| Load/store optimization | **Partial** | Dynamic rendering `loadOp`/`storeOp` paths. |
| Graphics + compute pipelines | **Yes** | `PipelineVK.cpp`. |
| RT / mesh pipelines | **No** | Not implemented. |
| Pipeline cache + disk | **Yes** | `DeviceVK` pipeline cache serialization. |
| Pipeline library / fast link | **No** | No `VK_EXT_graphics_pipeline_library` in device extension list. |
| Dynamic + extended dynamic state | **Partial** | Viewport/scissor dynamic; `VK_EXT_extended_dynamic_state` optional. |
| Descriptor pools / sets | **Yes** | `descriptors/*`. |
| Descriptor indexing (update-after-bind, partially bound) | **Yes** | `DeviceVK.cpp` feature chain + `BindlessHeapVK`. |
| Descriptor buffers | **No** | No `VK_EXT_descriptor_buffer`. |
| Push descriptors | **No** | Push **constants** yes; not `VK_KHR_push_descriptor`. |
| Timeline semaphores | **Yes** | Created and used in submit path (`DeviceVK.cpp`). |
| `VK_KHR_synchronization2` | **No** | Not in enabled extension list. |
| Async compute + transfer overlap | **Partial** | Queues may exist; submission path reviewed is graphics-centric. |
| Subpass dependencies / tile-local flow | **No** | No `VkRenderPass` subpass graph in this path. |
| Events (`vkCmdSetEvent2` / `WaitEvents2`) | **No** | Not used in audited code. |
| `VkQueryPool` / timestamps / occlusion / statistics | **Partial** | **Timestamps:** `vkCreateQueryPool`, pair `vkCmdWriteTimestamp` in `BeginQuery`/`EndQuery`, `GetResult` → ns; gated by **`supportsGpuTimestampQueries`**. Reset: `vkCmdResetQueryPool` (**outside** dynamic rendering) or **`vkResetQueryPool`** on host when **`supportsHostQueryReset`** (`IDevice::ResetQueryPoolOnHost`; **`BeginQuery`(Timestamp)** / **`ResetQueryPool`** use host path when enabled). **Occlusion:** `VK_QUERY_TYPE_OCCLUSION`, `vkCmdBeginQuery` / `vkCmdEndQuery`, `GetResult` raw count; gated by **`supportsOcclusionQueries`**. **Pipeline statistics** not exposed yet. |
| Ray tracing (BLAS/TLAS, pipelines, ray query) | **No** | Caps off; no AS or RT pipeline code in `rhi`. |
| Mesh shading | **No** | No mesh shader extension usage. |
| GPU-driven indirect (MDI, count buffers, scene buffers) | **Partial** | Indirect + optional **count** draws when `drawIndirectCount` enabled; BDA / scene buffer conventions still app-level. |
| Variable rate shading | **No** | No `VK_KHR_fragment_shading_rate`. |
| Subgroup / wave / fp16 / cooperative matrix / shader objects | **No** | Not enabled in `DeviceVK` init chain; shader code may still use baseline subgroup ops if device supports. |
| Swapchain modes + maintenance / HDR / present timing | **Partial** | Selectable FIFO/Mailbox/Immediate + surface **cap bits** in `RHICapabilities`; no `VK_EXT_swapchain_maintenance1`, HDR surface selection, or present-timing extensions yet. |
| Robustness / device fault / pipeline ISA introspection | **Partial** | Debug names (`DebugUtilsVK`); no `VK_EXT_device_fault` / `VK_KHR_pipeline_executable_properties` in list. |
| Portability / MoltenVK | **No** | Not a focus of this Windows-oriented path. |

### Cross-cutting (“modern pillars” from `Must_have_stuff.md`)

| Pillar | Status | Notes |
|--------|--------|-------|
| Bindless resources | **Partial** | Descriptor indexing + `BindlessHeapVK`; not descriptor-buffer bindless. |
| Render graph + auto-barriers | **Partial** | Tracker only; no graph executor in `rhi`. |
| Timeline semaphores | **Yes** | Device timeline + fence integration. |
| GPU-driven indirect rendering | **Partial** | Vulkan indirect path implemented; `supportsGpuDrivenPerDrawTransformIndirect` still depends on shaders/bindings. |
| Mesh shading | **No** | — |
| Ray tracing | **No** | — |
| Offline shader pipeline + PSO caching | **Partial** | GLSL→SPIR-V + `VkPipelineCache` disk persistence. |
