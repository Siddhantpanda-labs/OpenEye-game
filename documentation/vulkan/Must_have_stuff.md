Here's a complete breakdown — both layers together, organized by domain:

---

## RHI Layer — Core Responsibilities

**Abstraction & API Design**
- Backend-agnostic, opaque handle-based API (no Vulkan/DX12/Metal types leak out)
- Multi-backend compile-time or vtable dispatch (Vulkan, D3D12, Metal, WebGPU)
- Typed, versioned handles with free-list slot maps (no raw pointers)
- Explicit resource lifetime with deferred, fence-tracked destruction

**Command Recording**
- Command buffer / command list abstraction with explicit begin/end
- Render pass abstraction (color attachments, depth, resolve, load/store ops)
- Compute and copy dispatch commands
- Indirect draw and indirect dispatch support
- Per-frame command allocator pools (no per-frame allocation contention)

**Resource Model**
- Buffers, textures (1D/2D/3D/cube/array), samplers, render targets
- Resource views: SRV, UAV, RTV, DSV abstraction
- Texture formats with sRGB/linear/compressed (BC, ASTC, ETC2) support
- Mip generation, cubemap faces, array slices

**Pipeline State**
- Graphics PSO: vertex layout, rasterizer, blend, depth-stencil, shaders
- Compute PSO: compute shader
- Mesh shader PSO (amplification + mesh + pixel)
- Ray tracing PSO (ray gen, hit groups, miss shaders)
- PSO caching and disk serialization

**Shader System**
- SPIR-V / DXIL / MSL shader compilation and reflection
- Offline shader compilation pipeline with hot-reload
- Shader permutation system with defines/variants
- Root signature / push constants / descriptor layout abstraction

**Descriptor / Binding Model**
- Bindless resource model (descriptor indexing, unbounded arrays)
- Persistent descriptor tables / descriptor heaps
- Dynamic uniform buffers, push constants
- Sampler abstraction and immutable sampler support

**Synchronization (RHI-level)**
- Render graph / frame graph with automatic barrier insertion
- Explicit GPU-GPU and CPU-GPU fence/semaphore API
- Resource state tracking (optional explicit or auto-tracked)

**Swap Chain & Presentation**
- Platform swap chain (HWND, Wayland, XCB, Metal layer)
- V-sync, mailbox, immediate present modes
- HDR output support (HDR10, scRGB)

---

## Vulkan Backend — Core Concepts

**Instance & Device**
- Instance creation with validation layers, debug messenger (VK_EXT_debug_utils)
- Physical device selection (discrete vs integrated, feature/extension queries)
- Logical device with multiple queue families (graphics, compute, transfer, present)
- Device feature negotiation (Vulkan 1.3 core + extension caps)

**Memory Management**
- Custom allocator over VkDeviceMemory (VMA-style, TLSF or slab strategy)
- Memory type classification: device-local, host-visible, host-cached, lazily-allocated
- Heap aliasing for transient render targets (saves huge memory on tilers)
- Upload ring buffers (staging, per-frame, per-thread)
- Buffer Device Address (BDA) for GPU-side pointer arithmetic
- Sparse / partially-resident textures (virtual texturing, streaming)

**Render Passes & Framebuffers**
- VkRenderPass2 with subpasses for on-chip data flow (critical for mobile/tilers)
- Dynamic rendering (VK_KHR_dynamic_rendering) — no pre-declared render passes
- VkFramebuffer with imageless framebuffer extension
- Load/store op optimization (DONT_CARE, CLEAR, LOAD, STORE)

**Pipelines**
- VkPipeline graphics, compute, ray tracing, mesh shader pipelines
- Pipeline cache (VkPipelineCache) with serialization to disk
- Pipeline libraries and fast-link (VK_EXT_graphics_pipeline_library)
- Dynamic state (viewport, scissor, stencil, depth bias, blend constants)
- Extended dynamic state (VK_EXT_extended_dynamic_state 1/2/3) to reduce PSO explosion

**Descriptors**
- Descriptor pool and descriptor set management
- VK_EXT_descriptor_indexing: unbounded arrays, update-after-bind, non-uniform indexing
- Descriptor buffers (VK_EXT_descriptor_buffer) — bindless via plain buffer reads
- Push constants and push descriptors (VK_KHR_push_descriptor)

**Synchronization**
- Timeline semaphores (VkSemaphoreTypeTimeline) for all cross-queue sync
- VK_KHR_synchronization2: fine-grained stage/access masks, split barriers
- Async compute queue + async DMA/transfer queue with overlap
- Subpass dependencies for tile-local data flow on mobile
- Event-based intra-command-buffer synchronization (vkCmdSetEvent2 / vkCmdWaitEvents2)
- Pipeline statistics and timestamp queries (VkQueryPool)

**Ray Tracing**
- BLAS/TLAS build and update (VK_KHR_acceleration_structure)
- Compaction of BLASes to save memory
- Ray tracing pipeline + shader binding table (SBT) management
- Inline ray tracing (VK_KHR_ray_query) in any shader stage

**Mesh Shading**
- VK_EXT_mesh_shader: task (amplification) + mesh shader pipelines
- Meshlet data layout (position + attribute streams, index buffer packing)
- GPU-driven culling in task shaders

**GPU-Driven Rendering**
- Indirect draw (vkCmdDrawIndirect, vkCmdDrawIndexedIndirectCount)
- Indirect compute dispatch (vkCmdDispatchIndirect)
- Multi-draw indirect (MDI) with draw count from GPU buffer
- Compute-based culling (frustum, occlusion, LOD selection) writing to indirect buffers
- GPU scene / object buffer with BDA or bindless indexing

**Variable Rate Shading (VRS)**
- VK_KHR_fragment_shading_rate: per-draw, per-tile, per-primitive shading rates
- Shading rate image (tile-based rate map)
- Combiner ops (keep, replace, min, max, mul)

**Shader Features**
- Subgroup operations (ballot, shuffle, vote, arithmetic) — VK_KHR_shader_subgroup_extended_types
- Wave intrinsics for inter-lane communication (prefix scan, reduction)
- 16-bit / 8-bit arithmetic in shaders (fp16, int8 — VK_KHR_shader_float16_int8)
- Cooperative matrices / tensor ops (VK_KHR_cooperative_matrix) for ML/DLSS-style workloads
- Shader object (VK_EXT_shader_object) for shader-level binding without full PSO

**Query & Profiling**
- Timestamp queries (per-pass GPU timing)
- Pipeline statistics queries (vertex count, fragment count, compute invocations)
- Occlusion queries for conditional rendering
- GPU crash / breadcrumb markers (VK_EXT_device_fault, AMD aftermath)

**Swap Chain & Presentation**
- VkSwapchainKHR with mailbox / FIFO / immediate modes
- VK_EXT_swapchain_maintenance1 for present fence and deferred destroy
- HDR surface format selection (VK_COLOR_SPACE_HDR10_ST2084_EXT, scRGB)
- Frame pacing and present timing (VK_GOOGLE_display_timing / VK_EXT_present_timing)

**Robustness & Debugging**
- Validation layers (VK_LAYER_KHRONOS_validation) with GPU-assisted validation
- VK_EXT_debug_utils: named objects, debug regions (RenderDoc-visible labels)
- VK_EXT_device_fault for post-crash GPU error reporting
- VK_KHR_pipeline_executable_properties for shader ISA inspection
- Portability layer / MoltenVK support for macOS/iOS targets

---

**The non-negotiable modern pillars across both layers:** bindless resources, render graph with auto-barriers, timeline semaphores, GPU-driven indirect rendering, mesh shading, ray tracing, and a robust offline shader pipeline with PSO caching. Everything else builds on top of those.