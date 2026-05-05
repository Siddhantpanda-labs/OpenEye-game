# Above-RHI handoff checklist (Vulkan)

**Audience:** `RenderWorld`, frame graph, and gameplay/render-pass authors using `IDevice` / `ICommandList` after the Vulkan maturity phases in `documentation/vulkan/steps_todo.md` (roughly Phases **1–8** plus later RHI slices).

## What **`IDevice` / RHI** aim to guarantee

- **Resource lifetime:** Create/destroy APIs; Vulkan path defers frees on a **ring-buffered fence** (`DeletionQueueVK` + `BeginFrame`); textures and acceleration-structure wrappers may retire CPU handles a few frames after destroy.
- **Queues and submission:** Graphics vs optional async compute routing, timeline ordering where implemented; **no hidden** “magic” submit from arbitrary threads—assume **single render thread** unless documented otherwise (`DeviceVK.h`).
- **Capabilities:** `GetCapabilities()` and `RenderBackendPolicy::*` reflect enabled extensions (bindless, mesh, VRS, ray query slice, device fault query, subgroup sizes, etc.).
- **Device loss:** `IsDeviceLost()` flips after `VK_ERROR_DEVICE_LOST` on tracked paths; optional **`VK_EXT_device_fault`** may emit a **second** structured log line from `vkGetDeviceFaultInfoEXT` (best-effort). **Recovery** is **full device teardown and re-init**—not automatic hot-swap.
- **Swapchain / present:** Acquire, layout, present, and optional present-wait helpers; see `Vulkan/swapchain_present_policy.md`.

## What **callers above RHI** must own

- **Pass graph and scheduling:** Which passes run, resource versions, and **when** barriers are required between passes (RHI exposes barriers; the graph decides *where*).
- **Shader / material binding model:** Which descriptor sets, push constants, and bindless handles match active pipelines (see `Vulkan/descriptor_binding_table.md`).
- **CPU-side object lifetimes:** Not holding stale `ITexture*` / `IBuffer*` / `IAccelerationStructure*` after destroy; respect **destroy AS before backing buffer** for placed acceleration structures (`AccelerationStructureDesc` in `RHITypes.h`).
- **HDR / tonemapping / product pacing:** Swapchain format choices and frame pacing policy beyond raw Vulkan caps.

## Related docs

- `documentation/vulkan/steps_todo.md` — engineering sequence and phase status.
- `documentation/vulkan/vulkan_check.md` — capability matrix and changelog.
- `Vulkan/dynamic_rendering_pass_contract.md`, `Vulkan/query_pool_dynamic_rendering.md` — command-list and pass contracts.
