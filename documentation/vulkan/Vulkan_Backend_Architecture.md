# Vulkan backend — architecture, reference discipline, and maturity roadmap

**Purpose:** A single ordered plan to **compare**, **check**, **learn**, and **fix** OpenEye’s Vulkan RHI until it is **clean, consistent, and industry-grade** (validation-sane, debuggable, documented invariants).

**How we use references (this is the plan):** We **do not** copy-paste third-party code into OpenEye. We **read** Diligent, Wicked Engine, and Khronos **Vulkan Samples** to understand **behavior, invariants, and ordering** (barriers, sync, dynamic vs render pass, presentation). We then **implement** the same ideas in **OpenEye’s RHI language**—`DeviceVK`, `CommandListVK`, `BarrierTrackerVK`, and the existing folder layout under `eyelib/rendering/rhi/Vulkan/`. References are **teachers**; OpenEye remains **authoritative** for names and structure.

**Non-goals:** We do **not** copy any third party’s **file names, folder layout, or public type names** into OpenEye. We **do** adopt **discipline**: clear responsibilities, explicit resource state, safe command recording, and predictable device/context behavior—patterns visible in proven codebases vendored under `EngineSource/thirdparty/` (see below) and in the [Vulkan Specification](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html).

**Canonical OpenEye Vulkan tree (this roadmap’s scope):**

```
EngineSource/eyelib/rendering/rhi/Vulkan/
  core/           Instance, physical device, logical device, queues, swapchain, frame context, presentation
  commands/       Command pools, command buffers, CommandListVK (recording API)
  resources/      Buffer, texture, render targets
  memory/         Allocator (VMA), upload heap, deletion queue
  barriers/       ResourceState, BarrierTrackerVK
  pipelines/      Shaders, layouts, PSOs, SPIR-V path, resource spaces
  descriptors/    Sets, allocators, cache, bindless
  sync/           Fences, queries
  utils/          Enums, debug utils
```

Portable RHI surface: `EngineSource/eyelib/rendering/rhi/RHI.h` (and related types).

### Hardening pass — implemented summary

- **Contracts:** Module headers state ownership, ordering, and GPU or frame semantics (core device, memory, sync, commands, resources, barriers, pipelines, descriptors, presentation, `DeviceVK` threading and lifetime).
- **Validation-oriented logging:** Critical paths log failures (`VkResult`, descriptor pool exhaustion, swapchain acquire or present, pipeline or shader compile, etc.) instead of failing silently where we audited.
- **Debug captures:** `VK_EXT_debug_utils` object names (device, swapchain, PSOs, resources when `debugName` is set) and optional `CommandListVK` push or pop debug-label regions.
- **Scope and status:** Work targets **`eyelib/rendering/rhi/Vulkan`** only. Consolidated outcomes are in the **Phase roadmap — implemented work** table below and the summary in [`vulkan_check.md`](vulkan_check.md). Release gates (multi-GPU validation, CI smoke tests) stay separate process.

---

## Third-party reference roots (in this repo)

Use these paths when auditing or learning; **license** terms of each project apply if you redistribute excerpts elsewhere—reading for implementation insight inside the engine tree is normal.

| Source | Path in OpenEye | What it’s good for |
|--------|-----------------|---------------------|
| **Diligent Engine** | `EngineSource/thirdparty/diligent/Graphics/` | Portable **RHI-shaped** API (`GraphicsEngine/interface/`), Vulkan **device/context/resource** mapping (`GraphicsEngineVulkan/`), **VulkanUtilities** helpers, descriptor/pipeline discipline. |
| **Wicked Engine** | `EngineSource/thirdparty/Wicked_engine/WickedEngine/` | A full **Vulkan device** in engine code—e.g. `wiGraphicsDevice_Vulkan.cpp` (barriers, render-pass construction, submission patterns). `wiGraphics.h` illustrates a **compact graphics API surface** (compare concepts to `RHI.h`, not signatures). |
| **Khronos Vulkan Samples** | `EngineSource/thirdparty/Vulkan_Samples/` | **Spec-aligned minimal samples**: under `samples/api/` (e.g. swapchain, textures, instancing) and `samples/extensions/` (e.g. `dynamic_rendering/` for `vkCmdBeginRendering`, other extensions as needed). Use as **correctness checks** for a single feature, not as an engine template. |

**Suggested phase → reference hints** (optional; load small slices when auditing that phase):

| Roadmap phase | Diligent | Wicked | Vulkan_Samples |
|---------------|----------|--------|----------------|
| 1 Device / queues | Instance, logical device, queues | Device init, queue selection | Foundation samples in `samples/api/` |
| 3–4 Sync / commands | `DeviceContext`, deferred rules | Submit, semaphores, command flow | `samples/api/swapchain_recreation` |
| 4 Dynamic rendering | Dynamic rendering in backend | Render pass / dynamic mix in device | `samples/extensions/dynamic_rendering/` |
| 6 Barriers | `RESOURCE_STATE`, transitions | Image/layout barriers in `wiGraphicsDevice_Vulkan.cpp` | Barrier-heavy samples as needed |
| 9 Swapchain / present | Swapchain code paths | Swapchain / present handling | `swapchain_recreation`, resize patterns |

**Reference material detail (Diligent):**

| Area | Where to look |
|------|----------------|
| Portable RHI contracts | `thirdparty/diligent/Graphics/GraphicsEngine/interface/` (`RenderDevice.h`, `DeviceContext.h`, resource headers) |
| Vulkan mapping | `thirdparty/diligent/Graphics/GraphicsEngineVulkan/` (`RenderDeviceVkImpl`, `DeviceContextVkImpl`, `*VkImpl`) |
| Low-level Vulkan helpers | `thirdparty/diligent/Graphics/GraphicsEngineVulkan/include/VulkanUtilities/` |

**Reference material detail (Wicked):**

| Area | Where to look |
|------|----------------|
| Vulkan backend implementation | `thirdparty/Wicked_engine/WickedEngine/wiGraphicsDevice_Vulkan.cpp` (search for barrier, `VkRenderPass`, `vkCmd`, etc.) |
| Graphics API surface (declarations) | `thirdparty/Wicked_engine/WickedEngine/wiGraphics.h` |

**Reference material detail (Khronos Vulkan Samples):**

| Area | Where to look |
|------|----------------|
| API samples | `thirdparty/Vulkan_Samples/samples/api/<sample_name>/` — README / `.cpp` next to each |
| Extension samples | `thirdparty/Vulkan_Samples/samples/extensions/<name>/` — e.g. dynamic rendering, shader object |
| Spec / validation | [Vulkan Specification](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html), Khronos validation layer messages (VUIDs) |

**Practical rule:** When using Cursor or manual review, open **this roadmap + the OpenEye file under audit + one reference file** for that topic. Loading entire third-party trees dilutes context; **small, targeted** reads match the phase you are hardening.

---

## How to use this document

1. **Implemented bulk pass:** See **Hardening pass — implemented summary** (top) and **Phase roadmap — implemented work** (table). Canonical backend: **`eyelib/rendering/rhi/Vulkan/`** (deprecated `eyelib/render/` out of scope here).
2. **Future changes:** Prefer **check → learn → fix** against the [Vulkan spec](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html) and small reference slices; append a line to **Progress log** and to [`vulkan_check.md`](vulkan_check.md).
3. If a change touches **public RHI** behavior, note it in `documentation/Render_Module/Render_check.md` when it affects the render module harness.

---

## Principles (industry-ready bar)

| Principle | Meaning for OpenEye |
|-----------|---------------------|
| **Device vs recording** | `IDevice` owns creation, queues, swapchain, pools; `ICommandList` / `CommandListVK` owns **recording** and **per-list** state. No ambiguous ownership of `VkCommandBuffer`. |
| **Explicit resource state** | Every use of an image/buffer in a pass should be reachable from **known** state → **required** state (barrier or dynamic rendering begin). `UNKNOWN` only where deliberately tracked internally. |
| **PSO completeness** | Graphics pipelines are **immutable** after creation; dynamic state is a **declared** subset (viewport, scissor, depth/stencil dynamic, etc.). |
| **Validation-first** | Debug builds run with **Khronos validation**; fixes are **spec-driven** (VUID), not silence-the-layer. |
| **Interop clarity** | If OpenEye exposes `VkDevice` / handles, semantics are **documented** (lifetime, non-owning imports). |
| **Deterministic frame lifecycle** | Acquire → record → submit → present; resource recycling and fence signals are **ordered** and **documented**. |

---

## Phase roadmap — implemented work (0–11)

Phased hardening **2026-04-12**: each step combined **header contracts**, **failure logging** where gaps were found, and **small fixes**. Original roadmap used **Check / Learn / Fix** against references (Diligent, Wicked, Khronos samples)—not copy-paste. **Authoritative code:** `eyelib/rendering/rhi/Vulkan/` (ignore deprecated `eyelib/render/rhi/Vulkan` unless you explicitly sync it).

| Phase | Focus | Implemented (short) |
|------:|--------|----------------------|
| 0 | Baseline | CMake: `OPENEYE_USE_VULKAN`, `OPENEYE_VULKAN_VALIDATION` (validation layer + `VK_EXT_debug_utils`), `OPENEYE_VULKAN_DEBUG_CLEAR_RED`. Instance or device needs documented (API up to 1.3, swapchain, dynamic rendering, features—see `InstanceVK.cpp`, `DeviceVK::Initialize`). |
| 1 | Device or queues | Startup logs (instance API version, GPU name or type or API, queue family indices, extensions); **present on graphics** queue documented. |
| 2 | Memory | VMA-only path documented or logged; upload alignment; deletion or upload fence ordering. |
| 3 | Sync | Frame fence or semaphores or timeline `FenceVK` documented; **`vkQueueSubmit`** errors logged. |
| 4 | Commands | Pool or buffer or `CommandListVK` docs; **`vkBeginCommandBuffer`** failures logged. |
| 5 | Resources | `BufferVK` / `TextureVK` / `RenderTargetVK` docs (VMA, layouts, UBO); optional **`vkGetPhysicalDeviceFormatProperties`** pre-check noted as future. |
| 6 | Barriers | `ResourceState` or `BarrierTrackerVK` contracts; image layout cache behavior on `Reset`. |
| 7 | Pipelines | Dynamic rendering PSO docs; graphics or compute create failures logged; shader compile failure logs on asset path. |
| 8 | Descriptors | Pool or cache or bindless docs; **`OUT_OF_POOL_MEMORY`** hint; bindless pool or alloc errors logged; debug names on descriptor cache paths. |
| 9 | Swapchain | Swapchain or present docs or logs (acquire, present, per-image semaphores, resize or `SUBOPTIMAL`). |
| 10 | Debug | `SetDebugName*` for device or swapchain; optional **`CommandListVK`** debug-label regions. |
| 11 | Ship | **`DeviceVK`**: single render thread and resource or swapchain lifetime; GPU validation or CI remains **manual** per release. |

---

## Progress log

| Date | Notes |
|------|-------|
| 2026-04-12 | Phases **0–11** completed as above; detail also in [`vulkan_check.md`](vulkan_check.md). |

---

## Summary

The table above orders topics from **foundation** through **presentation** and **ship-style** notes. The **2026-04-12** pass covered all rows; further work should still use **check → learn → fix** rather than speculative rewrites.

**References:** OpenEye’s layout and naming stay **ours**. We **solidify** the Vulkan backend by comparing against **Diligent** (RHI discipline and Vulkan mapping), **Wicked Engine** (end-to-end device patterns in `wiGraphicsDevice_Vulkan.cpp`), and **Khronos Vulkan Samples** (minimal spec-correct feature samples)—then **re-implement** in OpenEye’s RHI. Final authority for correctness remains the **Vulkan specification** and **validation layers**, not any single engine.
