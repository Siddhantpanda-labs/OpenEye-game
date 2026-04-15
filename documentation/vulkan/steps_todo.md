# Vulkan RHI — implementation order (`steps_todo.md`)

**Purpose:** Single **ordered** checklist for what to build next under **`EngineSource/eyelib/rendering/rhi/`** (Vulkan backend under `Vulkan/`). Use this to drive **RHI + Vulkan maturity** until the path is safe to build **above-RHI** rendering (frame graph, passes, materials) without constant leakage of Vulkan details.

**Companion:** **`vulkan_check.md`** — capability matrix + changelog; append meaningful rows when work **lands**.

**Scope (locked):**

- **`eyelib/rendering/rhi/`** (+ Vulkan-only code this module owns). **Do not** expand this file into **`eyelib/render/`** (legacy).
- **No second graphics backend** in this roadmap (no D3D12/Metal/WebGPU implementation track here). `RHITypes` / `IDevice` may stay backend-agnostic; **this checklist is Vulkan-first maturity**, not multi-backend parity.
- **`documentation/Resume.md`** remains the product roadmap; **this file** is the **engineering sequence** for the Vulkan slice.

**How to use:** Implement **top to bottom** within each phase when possible; some later phases can start in parallel after **Phase 1** clears. Mark `[x]` when done; append dated notes. When a phase is fully `[x]`, refresh **`vulkan_check.md`** snapshot rows that moved **Partial → Yes** (or document intentional **Partial**).

---

## Baseline already landed (do not re-implement)

Summarized for continuity (detail lives in **`vulkan_check.md`** changelog + matrix):

- Descriptor-buffer **infra + bindless** (graphics + default compute path), mixed classic/DB fallbacks, barrier size hygiene (**old A.1–A.3, A.5**).
- **GPL** monolithic create with **`VkGraphicsPipelineLibraryCreateInfoEXT`** when supported (**old B**); **split + fast-link** still optional future slice.
- **Async compute** queue routing + ownership bridge + **`AsyncComputeSubmitPolicy`** (**old C**).
- **Present pacing** helpers + present-wait integration surface (**old D**).
- **Device fault / pipeline executable** optional enable + caps; **`IsDeviceLost`** + contract (**old E**).

**Still open from that era:** **descriptor-buffer validation pass** — now **Phase 1.1** below.

---

## Phase 1 — Quality gate (before “RHI is trusted”)

| Step | Status | Item |
| ---- | ------ | ---- |
| 1.1 | [ ] | **Validation layers (Debug):** Run **`EyelibRendering`** (and primary app target) with **Vulkan Validation Layers** on; fix **VU** / best-practice issues for **descriptor buffers**, bindless layouts, mixed **`vkCmdBindDescriptorSets`** / **`vkCmdBindDescriptorBuffersEXT`**, dynamic rendering + image layouts, and **swapchain acquire → present** chains. Close gaps until sessions are clean on **representative** scenes (load, resize, HDR toggle if used). |
| 1.2 | [ ] | **`OPENEYE_VULKAN_VALIDATION` / debug utils:** Confirm **`InstanceVK`** path is the one CI/devs use; document env/CMake toggles in one line here + pointer in **`vulkan_check.md`**. |
| 1.3 | [ ] | **`vulkan_check.md`:** After **1.1**, move **Descriptor buffers** matrix row toward **Yes** (or document remaining **Partial** reasons). |

---

## Phase 2 — Descriptor buffers & bindless → production-grade

**Goal:** Matrix row **Descriptor buffers** + **Bindless** read as **Yes** for the paths you ship, not “works in the lab.”

| Step | Status | Item |
| ---- | ------ | ---- |
| 2.1 | [ ] | **Layout coverage audit:** Enumerate every **`PipelineLayoutKind`** / compute layout that must bind bindless; ensure each has a **documented** DB vs classic path (or explicit “no bindless slot”). |
| 2.2 | [ ] | **Failure modes:** OOM on DB backing, zero-size heap, resize mid-frame — deterministic logs + safe fallback (classic sets or skip bind) without UB. |
| 2.3 | [ ] | **Hot path cost:** Optional profiling hooks (timestamps around heavy **`BindBindlessHeap`** / first use) — keep behind compile flag or existing stats. |
| 2.4 | [ ] | **`vulkan_check.md`:** Update bindless / DB notes when **2.1–2.3** land. |

---

## Phase 3 — Resource state, barriers, and GPU↔CPU visibility

**Goal:** Strengthen **`BarrierTrackerVK`** + explicit **`CommandListVK`** barriers until “who owns layout” is boringly correct under load and resize.

| Step | Status | Item |
| ---- | ------ | ---- |
| 3.1 | [ ] | **Audit pass:** Cross-check **`TransitionTextureToState`**, **`BarrierTexture`**, **`FlushBarrierTracker`**, and swapchain **`SetImageState`** / **`RemoveImageState`** against real passes (HDR resolve, depth copies, async compute handoff). |
| 3.2 | [ ] | **Redundant / contradictory barriers:** Extend coalesce + warn policy where it reduces noise without hiding bugs; document “expected redundant” cases (e.g. repeated `Present` layout). |
| 3.3 | [ ] | **Host-visible / upload:** Single story for staging visibility ( **`UploadHeapVK`** + buffer init ) vs **device-lost** contract — ensure **`NoteVulkanDeviceLost`** coverage on any new **`vkQueueSubmit`** sites. |
| 3.4 | [ ] | **`vulkan_check.md`:** Refresh **Resource state tracking** / **Render graph + auto barriers** rows. |

---

## Phase 4 — Synchronization, queues, and frames-in-flight

**Goal:** Make **Partial** rows (**Fences/semaphores**, **Multiple queue families**, **Async compute**) honest at AAA level: documented ordering, minimal surprise, optional **transfer** use.

| Step | Status | Item |
| ---- | ------ | ---- |
| 4.1 | [ ] | **Document invariants:** One-page comment block (or subsection here): **timeline** vs binary semaphores, **EndFrame** fence, **deferred** submits, **present** waits — what the render thread may assume. |
| 4.2 | [ ] | **Transfer queue (optional):** If **`QueuesVK`** exposes a dedicated **transfer** family, route **staging uploads** or **copy-only** work when safe (no hidden graphics dependency); else document “graphics-only transfer.” |
| 4.3 | [ ] | **Deferred submit stress:** Long runs with **async compute** on + ownership bridge; verify no ordering holes under **ForceGraphicsQueue** policy toggles. |
| 4.4 | [ ] | **`vulkan_check.md`:** Update **Async compute**, **Fences/semaphores**, **Multiple queue families** rows. |

---

## Phase 5 — Shaders, SPIR-V, and pipeline industrialization

**Goal:** Shippable **SPIR-V** path: errors, caching, and optional introspection — matrix **SPIR-V / shader compile** toward **Yes**.

| Step | Status | Item |
| ---- | ------ | ---- |
| 5.1 | [ ] | **Diagnostics:** Surface **glslang** / compile failures with **logical shader name**, stage, and **first error line** where available. |
| 5.2 | [ ] | **Pipeline cache versioning:** Disk cache path invalidates safely when **`DeviceVK`** feature set or driver changes (hash in filename or header blob). |
| 5.3 | [ ] | **GPL fast-link (optional):** Split libraries + link only if **5.1–5.2** stable and profiling justifies cost (see **Pipeline library / fast link** matrix). |
| 5.4 | [ ] | **`VK_KHR_pipeline_executable_properties` usage:** When cap on, expose minimal **RHI or dev-only** query (e.g. pipeline name / stage stats) — no hard dependency on shipping UI. |
| 5.5 | [ ] | **`vulkan_check.md`:** SPIR-V / pipeline cache / pipeline executable rows. |

---

## Phase 6 — Dynamic rendering & pass contract hardening

**Goal:** **`vkCmdBeginRendering`** path is predictable for higher-level passes (load/store, clears, MSAA if used).

| Step | Status | Item |
| ---- | ------ | ---- |
| 6.1 | [ ] | **Attachment contract:** Validate **`PipelineDesc::attachmentFormats`** vs **`VkPipelineRenderingCreateInfo`** at create time; fail fast with readable logs. |
| 6.2 | [ ] | **Load/store / MSAA:** Audit **`RenderPass`** abstraction gap (matrix **Partial**): document what the **frame graph above RHI** must specify vs what **`CommandListVK`** defaults. |
| 6.3 | [ ] | **`vulkan_check.md`:** **Render pass abstraction** / **Load/store** rows. |

---

## Phase 7 — Queries, timestamps, and profiling completeness

**Goal:** Matrix **`VkQueryPool` / timestamps…** from **Partial** to **Yes** for Vulkan-only shipping expectations.

| Step | Status | Item |
| ---- | ------ | ---- |
| 7.1 | [ ] | **Reset / begin / end rules:** Single doc for **dynamic rendering** + **host reset** interactions; ensure **`RenderBackendPolicy`** gates match **`CommandListVK`** enforcement. |
| 7.2 | [ ] | **Timestamp calibration:** Document **`timestampPeriod`** usage for **µs/ms** conversion in **`GpuProfiler`** consumers. |
| 7.3 | [ ] | **`vulkan_check.md`:** Query row tightening. |

---

## Phase 8 — Swapchain, HDR, and present robustness

**Goal:** **Swapchain modes + maintenance / HDR / present timing** matrix moves toward **Yes** for shipped configurations.

| Step | Status | Item |
| ---- | ------ | ---- |
| 8.1 | [ ] | **Resize / OUT_OF_DATE / SUBOPTIMAL:** Centralize policy (when **`Resize`** vs ignore); avoid redundant **`vkDeviceWaitIdle`** unless required. |
| 8.2 | [ ] | **HDR / color space:** Verify mode transitions (**`SwapchainOutputMode`**) on real HDR displays; document unsupported fallbacks. |
| 8.3 | [ ] | **Present timing:** Frame loop recipe using **`WaitForPreviousPresent`** + **`PresentWaitTimeoutNsFromTargetHz`** for latency caps (link **Phase D** doc). |
| 8.4 | [ ] | **`vulkan_check.md`:** Swapchain / HDR / present row. |

---

## Phase 9 — Optional mesh shading (`VK_EXT_mesh_shader`)

**Goal:** Matrix **Mesh shading** → **Partial** then **Yes**: optional parallel path, classic vertex pipeline remains default.

| Step | Status | Item |
| ---- | ------ | ---- |
| 9.1 | [ ] | **Device init:** Probe + enable **`VK_EXT_mesh_shader`** (or **`VK_NV_mesh_shader`** if targeting NVIDIA-only — pick one strategy and document). |
| 9.2 | [ ] | **Caps:** **`RHICapabilities`** + **`RenderBackendPolicy`** for mesh task/mesh draw support limits. |
| 9.3 | [ ] | **RHI surface:** Minimal **`IPipeline`** / create path (or dedicated **`CreateMeshPipeline`**) + **`CommandListVK::DrawMeshTasks`** (names illustrative — match engine conventions). |
| 9.4 | [ ] | **SPIR-V / shader path:** Asset convention for **`.mesh`** / **`.task`** stages in **`VulkanGlslToSpirv`** (or precompiled SPIR-V). |
| 9.5 | [ ] | **`vulkan_check.md`:** Mesh shading row + changelog. |

---

## Phase 10 — Variable rate shading (`VK_KHR_fragment_shading_rate`)

**Goal:** Matrix **Variable rate shading** → **Partial**/**Yes** for supported hardware.

| Step | Status | Item |
| ---- | ------ | ---- |
| 10.1 | [ ] | **Device init + caps:** Shading rate image / attachment / combiner ops per device limits. |
| 10.2 | [ ] | **Dynamic state:** `**vkCmdSetFragmentShadingRateKHR**` (or attachment / combiner path you standardize on). |
| 10.3 | [ ] | **RHI hook:** Small **`ICommandList`** / pass flag surface so **above-RHI** can set rate per pass without raw Vulkan in gameplay code. |
| 10.4 | [ ] | **`vulkan_check.md`:** VRS row. |

---

## Phase 11 — Ray tracing (large, optional until product commits)

**Goal:** Matrix **Ray tracing** → **Partial**/**Yes**: acceleration structures + minimal **`raygen`** / **`closesthit`** path **or** documented **`VK_KHR_ray_query`**-only slice.

| Step | Status | Item |
| ---- | ------ | ---- |
| 11.1 | [ ] | **Choose API slice:** RT pipeline (**`VK_KHR_ray_tracing_pipeline`**) vs **ray query** in compute/mesh — document tradeoff; implement **one** first. |
| 11.2 | [ ] | **Device init:** AS features, pipeline properties, shader binding table alignment. |
| 11.3 | [ ] | **Resources:** BLAS/TLAS buffers, scratch, instance buffer — **`RHITypes`** handles + **`DeviceVK`** create/destroy. |
| 11.4 | [ ] | **Commands:** Build AS, barrier AS, bind pipeline, trace rays / traceRaysIndirect (subset). |
| 11.5 | [ ] | **Safety:** Strict lifetimes (deferred delete with GPU fence) matching **`DeletionQueueVK`** patterns. |
| 11.6 | [ ] | **`vulkan_check.md`:** RT row + changelog. |

---

## Phase 12 — Robustness, tooling, and “above RHI” handoff

**Goal:** Close remaining **Partial** introspection / fault rows; give frame graph / renderer a **clear contract**.

| Step | Status | Item |
| ---- | ------ | ---- |
| 12.1 | [ ] | **`vkGetDeviceFaultInfoEXT`** (when **`VK_EXT_device_fault`** enabled): log structured fault on **`IsDeviceLost`** path (best-effort). |
| 12.2 | [ ] | **Subgroup / wave optional caps** (if shaders need them): expose **`VkPhysicalDeviceSubgroupProperties`**-derived limits in **`RHICapabilities`** (no shader change required in this step). |
| 12.3 | [ ] | **Handoff doc:** Short “**Above RHI** checklist” — what **`RenderWorld` / frame graph** must own vs what **`IDevice`** guarantees after Phases **1–8**. |
| 12.4 | [ ] | **`vulkan_check.md`:** Robustness / subgroup rows as applicable. |

---

## Explicitly out of scope for this file

- **Second graphics backend** (D3D12, Metal, WebGPU) implementation.
- **MoltenVK / portability** as a goal (Windows-first Vulkan is assumed unless product changes).
- **Classic `VkRenderPass` / subpass** graphs unless you deliberately pivot the architecture (dynamic rendering stays primary).

---

## Changelog of *this* file (meta)

| Date       | Change |
| ---------- | ------ |
| 2026-04-15 | Initial `steps_todo.md`: Phase A–F ordered from **Resume.md** + descriptor-buffer **Partial** state. |
| 2026-04-15 | Phases **A–E** implemented per prior meta rows; **A.4** left manual. **F** (VRS/mesh) was a short defer list. |
| 2026-04-15 | **Rewrote roadmap:** single **Phase 1–12** sequence aligned to **`vulkan_check.md`** gaps — quality gate → DB/bindless → barriers → sync/queues → shaders/pipelines → dynamic rendering → queries → swapchain/HDR/present → **mesh** → **VRS** → **ray tracing** → robustness + above-RHI handoff. Removed multi-backend work items. |
