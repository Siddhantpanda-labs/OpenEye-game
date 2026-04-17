# Vulkan RHI — implementation order (`steps_todo.md`)

**Purpose:** Single **ordered** checklist for what to build next under `**EngineSource/eyelib/rendering/rhi/`** (Vulkan backend under `Vulkan/`). Use this to drive **RHI + Vulkan maturity** until the path is safe to build **above-RHI** rendering (frame graph, passes, materials) without constant leakage of Vulkan details.

**Companion:** `**vulkan_check.md`** — capability matrix + changelog; append meaningful rows when work **lands**.

**Scope (locked):**

- `**eyelib/rendering/rhi/`** (+ Vulkan-only code this module owns). **Do not** expand this file into `**eyelib/render/`** (legacy).
- **No second graphics backend** in this roadmap (no D3D12/Metal/WebGPU implementation track here). `RHITypes` / `IDevice` may stay backend-agnostic; **this checklist is Vulkan-first maturity**, not multi-backend parity.
- `**documentation/Resume.md`** remains the product roadmap; **this file** is the **engineering sequence** for the Vulkan slice.

**How to use:** Implement **top to bottom** within each phase when possible; some later phases can start in parallel after **Phase 1** clears. Mark `[x]` when done; append dated notes. When a phase is fully `[x]`, refresh `**vulkan_check.md`** snapshot rows that moved **Partial → Yes** (or document intentional **Partial**).

**Discipline (agents + contributors):** After Vulkan RHI changes, **always verify with a Debug build of `EyelibRendering`**. Treat each phase as **conceptually and architecturally** correct, **complete** for the stated slice, and **coherent** with adjacent phases and docs—not “patch until it compiles” in Release only. Broader stress runs may come later and surface follow-up issues.

example - you can do  cd D:\Project\World3d\OpenEye; cmake --build build --target EyelibRendering

**Where docs live:** `**documentation/vulkan/`** holds this checklist + `**vulkan_check.md**` (discipline / matrix). **Module architecture** for the Vulkan RHI tree lives beside the code as `**EngineSource/eyelib/rendering/rhi/Vulkan/*.md`** (same pattern as `**descriptor_binding_table.md**`).

---

## Baseline already landed (do not re-implement)

Summarized for continuity (detail lives in `**vulkan_check.md`** changelog + matrix):

- Descriptor-buffer **infra + bindless** (graphics + default compute path), mixed classic/DB fallbacks, barrier size hygiene (**old A.1–A.3, A.5**).
- **GPL** monolithic create with `**VkGraphicsPipelineLibraryCreateInfoEXT`** when supported (**old B**); **split + fast-link** still optional future slice.
- **Async compute** queue routing + ownership bridge + `**AsyncComputeSubmitPolicy`** (**old C**).
- **Present pacing** helpers + present-wait integration surface (**old D**).
- **Device fault / pipeline executable** optional enable + caps; `**IsDeviceLost`** + contract (**old E**).

**Still open from that era:** **descriptor-buffer validation pass** — now **Phase 1.1** below.

---

## Phase 1 — Quality gate (before “RHI is trusted”)


| Step | Status | Item                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| ---- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1.1  | [ ]    | **Validation layers (Debug):** Run `**EyelibRendering`** (and primary app target) with **Vulkan Validation Layers** on; fix **VU** / best-practice issues for **descriptor buffers**, bindless layouts, mixed `**vkCmdBindDescriptorSets`** / `**vkCmdBindDescriptorBuffersEXT`**, dynamic rendering + image layouts, and **swapchain acquire → present** chains. Close gaps until sessions are clean on **representative** scenes (load, resize, HDR toggle if used). |
| 1.2  | [ ]    | `**OPENEYE_VULKAN_VALIDATION` / debug utils:** Confirm `**InstanceVK`** path is the one CI/devs use; document env/CMake toggles in one line here + pointer in `**vulkan_check.md`**.                                                                                                                                                                                                                                                                                   |
| 1.3  | [ ]    | `**vulkan_check.md`:** After **1.1**, move **Descriptor buffers** matrix row toward **Yes** (or document remaining **Partial** reasons).                                                                                                                                                                                                                                                                                                                               |


---

## Phase 2 — Descriptor buffers & bindless → production-grade

**Goal:** Matrix row **Descriptor buffers** + **Bindless** read as **Yes** for the paths you ship, not “works in the lab.” **Engineering slice (2.1–2.4) is complete:** layout matrix, failure handling, profile hook, and `**vulkan_check.md`** notes are updated. Rows remain **Partial** until `**Phase 1.1`** validation is clean on representative DB + bindless workloads (then promote matrix to **Yes**).


| Step | Status | Item                                                                                                                                                                                                |
| ---- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2.1  | [x]    | **Layout coverage audit:** Enumerate every `**PipelineLayoutKind`** / compute layout that must bind bindless; ensure each has a **documented** DB vs classic path (or explicit “no bindless slot”). |
| 2.2  | [x]    | **Failure modes:** OOM on DB backing, zero-size heap, resize mid-frame — deterministic logs + safe fallback (classic sets or skip bind) without UB.                                                 |
| 2.3  | [x]    | **Hot path cost:** Optional profiling hooks (timestamps around heavy `**BindBindlessHeap`** / first use) — keep behind compile flag or existing stats.                                              |
| 2.4  | [x]    | `**vulkan_check.md`:** Update bindless / DB notes when **2.1–2.3** land.                                                                                                                            |


---

## Phase 3 — Resource state, barriers, and GPU↔CPU visibility

**Goal:** Strengthen `**BarrierTrackerVK`** + explicit `**CommandListVK`** barriers until “who owns layout” is boringly correct under load and resize.

**Engineering slice (3.1–3.4) landed 2026-04-16:** tracker `**SetImageState` / `RemoveImageState`** now clear **pending** image barriers (parity with buffers); `**BarrierTexture`** validates caller `**fromState`** vs cached layout (warn, tracker wins); one-shot upload `**vkQueueSubmit**` paths call `**NoteVulkanDeviceLost**`; matrix + `**BarrierTrackerVK.h**` document redundant / external-barrier policy.


| Step | Status | Item                                                                                                                                                                                                                                            |
| ---- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 3.1  | [x]    | **Audit pass:** Cross-check `**TransitionTextureToState`**, `**BarrierTexture`**, `**FlushBarrierTracker**`, and swapchain `**SetImageState**` / `**RemoveImageState**` against real passes (HDR resolve, depth copies, async compute handoff). |
| 3.2  | [x]    | **Redundant / contradictory barriers:** Extend coalesce + warn policy where it reduces noise without hiding bugs; document “expected redundant” cases (e.g. repeated `Present` layout).                                                         |
| 3.3  | [x]    | **Host-visible / upload:** Single story for staging visibility ( `**UploadHeapVK`** + buffer init ) vs **device-lost** contract — ensure `**NoteVulkanDeviceLost`** coverage on any new `**vkQueueSubmit`** sites.                              |
| 3.4  | [x]    | `**vulkan_check.md`:** Refresh **Resource state tracking** / **Render graph + auto barriers** rows.                                                                                                                                             |


---

## Phase 4 — Synchronization, queues, and frames-in-flight

**Goal:** Make **Partial** rows (**Fences/semaphores**, **Multiple queue families**, **Async compute**) honest at AAA level: documented ordering, minimal surprise, optional **transfer** use.

**Implementation status (code/design-first):** **4.1**, **4.2**, and **4.4** are now covered in-source/docs; **4.3** remains a later runtime stress pass by intent.


| Step | Status | Item                                                                                                                                                                                                                         |
| ---- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 4.1  | [x]    | **Document invariants:** One-page comment block (or subsection here): **timeline** vs binary semaphores, **EndFrame** fence, **deferred** submits, **present** waits — what the render thread may assume.                    |
| 4.2  | [x]    | **Transfer queue (optional):** If `**QueuesVK`** exposes a dedicated **transfer** family, route **staging uploads** or **copy-only** work when safe (no hidden graphics dependency); else document “graphics-only transfer.” |
| 4.3  | [ ]    | **Deferred submit stress:** Long runs with **async compute** on + ownership bridge; verify no ordering holes under **ForceGraphicsQueue** policy toggles.                                                                    |
| 4.4  | [x]    | `**vulkan_check.md`:** Update **Async compute**, **Fences/semaphores**, **Multiple queue families** rows.                                                                                                                    |


---

## Phase 5 — Shaders, SPIR-V, and pipeline industrialization

**Goal:** Shippable **SPIR-V** path: errors, caching, and optional introspection — matrix **SPIR-V / shader compile** toward **Yes**.


| Step | Status | Item                                                                                                                                                                                                  |
| ---- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 5.1  | [x]    | **Diagnostics:** Surface **glslang** / compile failures with **logical shader name**, stage, and **first error line** where available.                                                                |
| 5.2  | [x]    | **Pipeline cache versioning:** Disk cache path invalidates safely when `**DeviceVK`** feature set or driver changes (hash in filename or header blob).                                                |
| 5.3  | [ ]    | **GPL fast-link (optional):** Split libraries + link only if **5.1–5.2** stable and profiling justifies cost (see **Pipeline library / fast link** matrix). **Deferred** until profiling asks for it. |
| 5.4  | [x]    | `**VK_KHR_pipeline_executable_properties` usage:** When cap on, expose minimal **RHI or dev-only** query (e.g. pipeline name / stage stats) — no hard dependency on shipping UI.                      |
| 5.5  | [x]    | `**vulkan_check.md`:** SPIR-V / pipeline cache / pipeline executable rows.                                                                                                                            |


---

## Phase 6 — Dynamic rendering & pass contract hardening

**Goal:** `**vkCmdBeginRendering`** path is predictable for higher-level passes (load/store, clears, MSAA if used).

**Engineering slice (6.1–6.3) landed 2026-04-16:** `**CreateVkGraphicsPipeline`** validates resolved `**VkPipelineRenderingCreateInfo**` vs device limits and `**PipelineDesc::depthStencil**`; pass contract + load/store defaults documented in `**EngineSource/eyelib/rendering/rhi/Vulkan/dynamic_rendering_pass_contract.md**`; matrix rows updated. MSAA remains single-sampled until a dedicated milestone.


| Step | Status | Item                                                                                                                                                                               |
| ---- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 6.1  | [x]    | **Attachment contract:** Validate `**PipelineDesc::attachmentFormats`** vs `**VkPipelineRenderingCreateInfo`** at create time; fail fast with readable logs.                       |
| 6.2  | [x]    | **Load/store / MSAA:** Audit `**RenderPass`** abstraction gap (matrix **Partial**): document what the **frame graph above RHI** must specify vs what `**CommandListVK`** defaults. |
| 6.3  | [x]    | `**vulkan_check.md`:** **Render pass abstraction** / **Load/store** rows.                                                                                                          |


---

## Phase 7 — Queries, timestamps, and profiling completeness

**Goal:** Matrix `**VkQueryPool` / timestamps…** from **Partial** to **Yes** for Vulkan-only shipping expectations.

**Engineering slice (7.1–7.3) landed 2026-04-16:** `**EngineSource/eyelib/rendering/rhi/Vulkan/query_pool_dynamic_rendering.md`** (reset / begin / end vs dynamic rendering, `**RenderBackendPolicy**` table, `**timestampPeriod**` → `**GetResult**` ns); `**DeviceVK::CreateQueryPool**` / `**ResetQueryPoolOnHost**` use `**RenderBackendPolicy**` helpers; `**CommandListVK.h**`, `**RHI.h**`, `**GpuProfiler.h**` cross-links; `**vulkan_check.md**` query rows refreshed.


| Step | Status | Item                                                                                                                                                                             |
| ---- | ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 7.1  | [x]    | **Reset / begin / end rules:** Single doc for **dynamic rendering** + **host reset** interactions; ensure `**RenderBackendPolicy`** gates match `**CommandListVK`** enforcement. |
| 7.2  | [x]    | **Timestamp calibration:** Document `**timestampPeriod`** usage for **µs/ms** conversion in `**GpuProfiler`** consumers.                                                         |
| 7.3  | [x]    | `**vulkan_check.md`:** Query row tightening.                                                                                                                                     |


---

## Phase 8 — Swapchain, HDR, and present robustness

**Goal:** **Swapchain modes + maintenance / HDR / present timing** matrix moves toward **Yes** for shipped configurations.

**Engineering slice (8.1–8.4) landed 2026-04-16:** `**EngineSource/eyelib/rendering/rhi/Vulkan/swapchain_present_policy.md`** — `**OUT_OF_DATE**` → `**Resize**`, `**SUBOPTIMAL**` → capped logs only, `**vkDeviceWaitIdle**` rationale on swapchain teardown, HDR `**SwapchainOutputMode**` fallback table (code in `**SwapchainVK::SelectSurfaceFormat**`), present-wait recipe + `**PresentWaitTimeoutNsFromTargetHz**`; `**DeviceVK::NoteSwapchainKhrResult**` centralizes acquire + present paths; `**vulkan_check.md**` rows + changelog.


| Step | Status | Item                                                                                                                                                       |
| ---- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 8.1  | [x]    | **Resize / OUT_OF_DATE / SUBOPTIMAL:** Centralize policy (when `**Resize`** vs ignore); avoid redundant `**vkDeviceWaitIdle`** unless required.            |
| 8.2  | [x]    | **HDR / color space:** Verify mode transitions (`**SwapchainOutputMode`**) on real HDR displays; document unsupported fallbacks.                           |
| 8.3  | [x]    | **Present timing:** Frame loop recipe using `**WaitForPreviousPresent`** + `**PresentWaitTimeoutNsFromTargetHz`** for latency caps (link **Phase D** doc). |
| 8.4  | [x]    | `**vulkan_check.md`:** Swapchain / HDR / present row.                                                                                                      |


---

## Phase 9 — Optional mesh shading (`VK_EXT_mesh_shader`)

**Goal:** Matrix **Mesh shading** → **Partial** then **Yes**: optional parallel path, classic vertex pipeline remains default.

**Engineering slice (9.1–9.5) landed 2026-04-16:** Cross-vendor `**VK_EXT_mesh_shader`** (not `**VK_NV_mesh_shader**`-only); `**DeviceVK**` feature + properties + `**GetMeshPushPipelineLayout**`; `**CreateVkMeshGraphicsPipeline**` (`PipelineVKMesh.cpp`) + `**PipelineDesc::taskShader`/`meshShader**`; `**IPipeline::UsesMeshShaders**`, `**ICommandList::DrawMeshTasks`/`DrawMeshTasksIndirect**`; `**VulkanGlslToSpirv**` / `**ShaderVK**` / `**ResolveShaderPath**` for `**.task`/`.mesh**`; `**mesh_shader_rhi.md**`; `**vulkan_check.md**`.

**2026-04-16 follow-up:** Resource-space `**VkPipelineLayout`** / descriptor `**stageFlags**` now include mesh when the device supports `**VK_EXT_mesh_shader**`; `**PipelineVK**` may create mesh pipelines with `**PipelineLayoutKind::Forward**` (and other graphics pass kinds), not `**PushOnly**` only.


| Step | Status | Item                                                                                                                                                                                 |
| ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 9.1  | [x]    | **Device init:** Probe + enable `**VK_EXT_mesh_shader`** (or `**VK_NV_mesh_shader`** if targeting NVIDIA-only — pick one strategy and document).                                     |
| 9.2  | [x]    | **Caps:** `**RHICapabilities`** + `**RenderBackendPolicy`** for mesh task/mesh draw support limits.                                                                                  |
| 9.3  | [x]    | **RHI surface:** Minimal `**IPipeline`** / create path (or dedicated `**CreateMeshPipeline`**) + `**CommandListVK::DrawMeshTasks**` (names illustrative — match engine conventions). |
| 9.4  | [x]    | **SPIR-V / shader path:** Asset convention for `**.mesh`** / `**.task`** stages in `**VulkanGlslToSpirv**` (or precompiled SPIR-V).                                                  |
| 9.5  | [x]    | `**vulkan_check.md`:** Mesh shading row + changelog.                                                                                                                                 |


---

## Phase 10 — Variable rate shading (`VK_KHR_fragment_shading_rate`)

**Goal:** Matrix **Variable rate shading** → **Partial**/**Yes** for supported hardware.

**Engineering slice (10.1–10.4) landed 2026-04-17:** Vulkan init now probes/enables `**VK_KHR_fragment_shading_rate`** (pipeline fragment shading-rate feature) and records min/max texel-size limits into `**RHICapabilities**`; `DeviceVK` loads `**vkCmdSetFragmentShadingRateKHR**`; `ICommandList` exposes backend-agnostic `**SetFragmentShadingRate**` with combiner ops and `CommandListVK` records the dynamic state call; `RenderBackendPolicy::SupportsVariableRateShading` and matrix/changelog rows were updated.


| Step | Status | Item                                                                                                                                   |
| ---- | ------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| 10.1 | [x]    | **Device init + caps:** Shading rate image / attachment / combiner ops per device limits.                                              |
| 10.2 | [x]    | **Dynamic state:** `**vkCmdSetFragmentShadingRateKHR`** (or attachment / combiner path you standardize on).                            |
| 10.3 | [x]    | **RHI hook:** Small `**ICommandList`** / pass flag surface so **above-RHI** can set rate per pass without raw Vulkan in gameplay code. |
| 10.4 | [x]    | `**vulkan_check.md`:** VRS row.                                                                                                        |


---

## Phase 11 — Ray tracing (large, optional until product commits)

**Goal:** Matrix **Ray tracing** → **Partial**/**Yes**: acceleration structures + minimal `**raygen`** / `**closesthit`** path **or** documented `**VK_KHR_ray_query`**-only slice.

**Engineering slice (11.1–11.6) landed 2026-04-17:** chose `**VK_KHR_ray_query`-first** (not RT pipeline/SBT yet). `DeviceVK` probes optional `**VK_KHR_deferred_host_operations` + `VK_KHR_acceleration_structure` + `VK_KHR_ray_query`**, enables the feature chain when supported, loads core acceleration-structure entry points (`vkCreate/DestroyAccelerationStructureKHR`, `vkCmdBuildAccelerationStructuresKHR`, build-size/device-address queries), and publishes caps via `RHICapabilities` (`supportsRayQuery`, `supportsAccelerationStructureBuild`, AS scratch alignment/primitive limits). `RenderBackendPolicy::SupportsRayQuery` / **`SupportsAccelerationStructureBuild`** were added. **11.3 — Resources:** `RHITypes` / `RHI.h` surface `**IAccelerationStructure**`, `**GetAccelerationStructureBuildSizes**`, `**CreateAccelerationStructure**` / `**DestroyAccelerationStructure**`; dedicated **`IAccelerationStructureScratchBuffer`** / **`IAccelerationStructureInstanceBuffer`** + `**AccelerationStructureBuffersVK**` (scratch size aligned to device scratch alignment; TLAS instance stride matches `VkAccelerationStructureInstanceKHR`); `**BufferUsage::AccelerationStructureStorage`** / **`AccelerationStructureBuildInput`**; `**BufferVK::InitFromVkUsage**` for shared VMA create; `**DeletionQueueVK**` destroys `**VkAccelerationStructureKHR**` before freeing backing buffers. **11.4 — Commands:** `**ICommandList::BuildAccelerationStructures**` / **`MemoryBarrierAccelerationStructureAccess**` on `**CommandListVK**`; `**ResourceState::AsBuildScratch` / `AsBuildGeometryInput` / `AsBuildStorage**` + `**VulkanEnums**` barrier mapping; `**IAccelerationStructure::GetNativeAccelerationStructure**`. **11.5 — Safety:** backing-`VkBuffer` refcount + capped error if `**DestroyBuffer**` while AS live; deferred `**AccelerationStructureVK**` wrapper delete (`**kMaxFrames**` + fence, same idea as textures); retired AS returns null native handle / zero device address. **11.6 — Docs:** `**vulkan_check.md`** ray-tracing rows + changelog.


| Step | Status | Item                                                                                                                                                 |
| ---- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| 11.1 | [x]    | **Choose API slice:** RT pipeline (`**VK_KHR_ray_tracing_pipeline`**) vs **ray query** in compute/mesh — document tradeoff; implement **one** first. |
| 11.2 | [x]    | **Device init:** AS features, pipeline properties, shader binding table alignment.                                                                   |
| 11.3 | [x]    | **Resources:** BLAS/TLAS buffers, scratch, instance buffer — `**RHITypes`** handles + `**DeviceVK`** create/destroy.                                 |
| 11.4 | [x]    | **Commands (ray-query slice):** `**vkCmdBuildAccelerationStructuresKHR**` + conservative AS **memory** barrier on `**ICommandList**`; buffer `**ResourceState**` for scratch / build-input / AS storage; `**GetNativeAccelerationStructure**`. **Deferred:** RTPSO bind + `**vkCmdTraceRaysKHR**` (**row above** / later phase). |
| 11.5 | [x]    | **Safety:** `**DeletionQueueVK**` AS-before-buffer order (documented); `**DeviceVK**` backing-buffer refcount + `**DestroyBuffer**` guard; deferred `**AccelerationStructureVK**` wrapper retirement + null `**GetNativeAccelerationStructure**` / address when retired. |
| 11.6 | [x]    | `**vulkan_check.md`:** RT row + changelog (with **11.5** lifetime notes).                                                                                                           |


---

## Phase 12 — Robustness, tooling, and “above RHI” handoff

**Goal:** Close remaining **Partial** introspection / fault rows; give frame graph / renderer a **clear contract**.

**Engineering slice (12.1–12.4) landed 2026-04-17:** `**vkGetDeviceFaultInfoEXT**` loaded when `**VK_EXT_device_fault**` is enabled; `**DeviceVK::NoteVulkanDeviceLost**` invokes best-effort structured fault logging after `**VK_ERROR_DEVICE_LOST**`. `**VkPhysicalDeviceSubgroupProperties**` queried in `**DeviceVK::QueryCapabilities**` into `**RHICapabilities**` + `**RenderBackendPolicy::SubgroupWaveSize**`. Handoff checklist: `**EngineSource/eyelib/rendering/rhi/above_rhi_handoff_checklist.md**`. `**vulkan_check.md`** robustness + subgroup rows updated.


| Step | Status | Item                                                                                                                                                                                        |
| ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 12.1 | [x]    | `**vkGetDeviceFaultInfoEXT`** (when `**VK_EXT_device_fault`** enabled): log structured fault on `**IsDeviceLost**` path (best-effort).                                                      |
| 12.2 | [x]    | **Subgroup / wave optional caps** (if shaders need them): expose `**VkPhysicalDeviceSubgroupProperties`**-derived limits in `**RHICapabilities`** (no shader change required in this step). |
| 12.3 | [x]    | **Handoff doc:** Short “**Above RHI** checklist” — what `**RenderWorld` / frame graph** must own vs what `**IDevice`** guarantees after Phases **1–8**.                                     |
| 12.4 | [x]    | `**vulkan_check.md`:** Robustness / subgroup rows as applicable.                                                                                                                            |


---

## Explicitly out of scope for this file

- **Second graphics backend** (D3D12, Metal, WebGPU) implementation.
- **MoltenVK / portability** as a goal (Windows-first Vulkan is assumed unless product changes).
- **Classic `VkRenderPass` / subpass** graphs unless you deliberately pivot the architecture (dynamic rendering stays primary).

---

## Changelog of *this* file (meta)


| Date       | Change                                                                                                                                                                                                                                                                                                                                                                           |
| ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-04-15 | Initial `steps_todo.md`: Phase A–F ordered from **Resume.md** + descriptor-buffer **Partial** state.                                                                                                                                                                                                                                                                             |
| 2026-04-15 | Phases **A–E** implemented per prior meta rows; **A.4** left manual. **F** (VRS/mesh) was a short defer list.                                                                                                                                                                                                                                                                    |
| 2026-04-15 | **Rewrote roadmap:** single **Phase 1–12** sequence aligned to `**vulkan_check.md`** gaps — quality gate → DB/bindless → barriers → sync/queues → shaders/pipelines → dynamic rendering → queries → swapchain/HDR/present → **mesh** → **VRS** → **ray tracing** → robustness + above-RHI handoff. Removed multi-backend work items.                                             |
| 2026-04-16 | Marked **Phase 2.3** done: optional `**OPENEYE_RHI_BINDLESS_PROFILE`** CMake option + `**BindBindlessHeap`** CPU timing logs; `**descriptor_binding_table.md**` + `**vulkan_check.md**` changelog updated.                                                                                                                                                                       |
| 2026-04-16 | **Phase 2 complete (2.1–2.4):** matrix snapshot + **Descriptor buffers** / **Bindless** / pillar notes refreshed; **Partial** retained with explicit **Phase 1.1** gate for **Yes**; checkboxes **2.1–2.4** marked done.                                                                                                                                                         |
| 2026-04-16 | **Phase 3 (3.1–3.4):** `**BarrierTrackerVK`** image pending clear on `**SetImageState`/`RemoveImageState`**; `**BarrierTexture**` `**fromState**` mismatch warn; `**NoteVulkanDeviceLost**` on `**TextureVK`/`BufferVK**` upload submit; `**vulkan_check.md**` + header comments. Rows stay **Partial** until deeper pass/resize stress (ongoing).                               |
| 2026-04-16 | **Phase 4 (code/design slice):** documented frame/submit invariants in `DeviceVK.h`; upload queue policy now checks transfer queue safely (`TextureVK`/`BufferVK`) and explicitly keeps dedicated transfer-family paths on graphics pending ownership-transfer implementation; matrix rows refreshed and **4.1/4.2/4.4** marked done (4.3 deferred by plan).                     |
| 2026-04-16 | **Phase 5 (5.1, 5.2, 5.4, 5.5):** shaderc failure logs include logical name + stage + first error line (+ optional full message **Info**); on-disk `**VkPipelineCache`** path includes `**pipelineCacheUUID`**; optional `**vkGetPipelineExecutablePropertiesKHR**` one-shot dev sample after pipeline create; `**vulkan_check.md**` updated. **5.3** (GPL fast-link) left open. |
| 2026-04-16 | **Discipline block:** verify Vulkan RHI work with **Debug** `**EyelibRendering`**; phases done when **conceptually / architecturally** sound, **complete** for the slice, and **coherent** with neighbors—not Release-only compile fixes.                                                                                                                                        |
| 2026-04-16 | **Phase 6 (6.1–6.3):** `**CreateVkGraphicsPipeline`** validates dynamic-rendering attachment contract vs limits + depth/stencil state; `**EngineSource/eyelib/rendering/rhi/Vulkan/dynamic_rendering_pass_contract.md**` (frame graph vs `**CommandListVK**` defaults, MSAA note); `**vulkan_check.md**` render pass / load-store rows.                                          |
| 2026-04-16 | **Phase 7 (7.1–7.3):** `**EngineSource/eyelib/rendering/rhi/Vulkan/query_pool_dynamic_rendering.md`**; `**DeviceVK::CreateQueryPool**` / `**ResetQueryPoolOnHost**` aligned with `**RenderBackendPolicy**`; `**vulkan_check.md**` query / host-reset notes.                                                                                                                      |
| 2026-04-16 | **Doc layout:** moved `**query_pool_dynamic_rendering.md`** and `**dynamic_rendering_pass_contract.md**` from `**documentation/vulkan/**` to `**EngineSource/eyelib/rendering/rhi/Vulkan/**` (architecture beside code); updated cross-links.                                                                                                                                    |
| 2026-04-16 | **Phase 8 (swapchain / HDR / present):** `**swapchain_present_policy.md`** + `**DeviceVK::NoteSwapchainKhrResult**`; `**vulkan_check.md**` matrix + changelog; `**RHITypes.h**` / `**DeviceVK.h**` cross-links.                                                                                                                                                                  |
| 2026-04-16 | **Phase 9 (mesh shading):** `**VK_EXT_mesh_shader`** in `**DeviceVK**`; `**PipelineVKMesh.cpp**` / `**PipelineDesc**` task+mesh; `**DrawMeshTasks**`; `**mesh_shader_rhi.md**`; caps + `**RenderBackendPolicy::SupportsMeshShader**`; `**vulkan_check.md**`.                                                                                                                     |
| 2026-04-16 | **Phase 9:** Mesh-capable **Forward/GBuffer/…** pipeline layouts (push + descriptor `**stageFlags`**) + `**PipelineVK**` mesh + non-`**PushOnly**` `**PipelineLayoutKind**`; `**PushGraphicsConstantsIfPossible**` fallback for **PushOnly** mesh.                                                                                                                               |
| 2026-04-17 | **Phase 10 (VRS):** `**VK_KHR_fragment_shading_rate`** probe+enable in `**DeviceVK**` with caps (min/max texel size), dynamic state entrypoint load (`**vkCmdSetFragmentShadingRateKHR**`), new `**ICommandList::SetFragmentShadingRate**` + Vulkan implementation, and `**RenderBackendPolicy::SupportsVariableRateShading**` / `**vulkan_check.md**` updates.                  |
| 2026-04-17 | **Phase 11 (ray query slice):** chose `**VK_KHR_ray_query`**-first; optional **AS + ray query** feature/extension enable in `**DeviceVK`**, acceleration-structure entrypoint load, `RHICapabilities` (`supportsRayQuery`, scratch alignment/primitive limit), and `**RenderBackendPolicy::SupportsRayQuery**`.                                                                  |
| 2026-04-17 | **Phase 11.3 (AS resources):** RHI `**IAccelerationStructure**` + build-size query + create/destroy; dedicated **`IAccelerationStructureScratchBuffer`** / **`IAccelerationStructureInstanceBuffer`** (`**AccelerationStructureBuffersVK**`); `**BufferVK::InitFromVkUsage**`; `**DeletionQueueVK**` tears down `**VkAccelerationStructureKHR**` before VMA buffer free; `**vulkan_check.md`** matrix + changelog updated for this slice. |
| 2026-04-17 | **Phase 11.4 (AS commands):** `**ICommandList::BuildAccelerationStructures**` / **`MemoryBarrierAccelerationStructureAccess**` (`**CommandListVK**`); `**RHICapabilities::supportsAccelerationStructureBuild**` + `**RenderBackendPolicy::SupportsAccelerationStructureBuild**`; AS-oriented `**ResourceState**` values + `**VulkanEnums**`; `**GetNativeAccelerationStructure**`; scratch/instance buffer barrier-tracker seeding via `**InitFromVkUsage**`; `**vulkan_check.md`** updated. |
| 2026-04-17 | **Phase 11.5–11.6 (AS lifetime + matrix):** `**DeviceVK**` tracks live AS per backing `**VkBuffer**`; `**DestroyBuffer**` logs if refcount > 0; `**DestroyAccelerationStructure**` defers wrapper delete (`**kMaxFrames**` + `**BeginFrame**` fence) like textures; `**AccelerationStructureVK**` retired → null native / zero address; `**DeletionQueueVK.h**` / `**RHITypes**` / `**RHI.h**` contract notes; `**vulkan_check.md`** ray-tracing rows + changelog. |
| 2026-04-17 | **Phase 12 (robustness + above-RHI):** `**vkGetDeviceFaultInfoEXT**` + `**NoteVulkanDeviceLost**` structured fault log; `**VkPhysicalDeviceSubgroupProperties**` → `**RHICapabilities**` + `**RenderBackendPolicy::SubgroupWaveSize**`; `**above_rhi_handoff_checklist.md**`; `**vulkan_check.md`** robustness + subgroup rows; **`steps_todo.md`** **12.1–12.4** **[x]**. |


