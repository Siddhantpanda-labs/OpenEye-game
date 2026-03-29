# Render Module Professional Audit Report

Date: 2026-03-29  
Scope: `EngineSource/eyelib/render` (297 files)

---

## 1) Purpose

This report provides:

- a complete structural audit of the render module,
- current runtime flow and subsystem connections,
- identified architectural pain points causing growth/messiness,
- and a professional target design (Unreal-style principles) for scalable, clean long-term evolution.

The intent is to define a clear **end-state architecture goal** and a practical standard to manage future growth without regressions.

---

## 2) Executive Assessment

The module already has strong foundations:

- multi-backend RHI (`OpenGL`, `Vulkan`, optional `Diligent`),
- frame graph core (`framegraph/`) with recipe wiring (`renderer/graph/`),
- pass-system extraction in progress (`*PassSystem` files),
- clear resource/material/shader subsystems,
- advanced features (deferred, forward, forward+, shadows, transparency, skybox, post, editor overlays, streaming).

Primary problem is not missing capability.  
Primary problem is **architecture concentration**:

- too much orchestration + pass detail + integration logic in `renderer/RenderPipeline.cpp`,
- mixed old/new ownership model (classical `*Pass` + newer `*PassSystem`),
- spread of backend decisions across layers,
- and a growing number of files without a strict ownership contract.

Conclusion:

- The module is technically advanced, but requires a strict **governance model + folder ownership model** to remain scalable and maintainable.

---

## 3) Current Layered Architecture (As-Is)

## 3.1 System layers

1. **Orchestration Layer**
   - `renderer/RenderPipeline.*`
   - `renderer/graph/*`
   - owns frame lifecycle, mode dispatch, graph compile/execute wiring.

2. **Frame Graph Layer**
   - `framegraph/FrameGraph.*`
   - `framegraph/FrameGraphRecipe.*`
   - DAG, pass dependencies, transient resources, barriers/transition ordering.

3. **Pass Execution Layer**
   - `passes/*`
   - legacy pass bodies + extracted pass systems (`DeferredLightingPassSystem`, `GBufferPassSystem`, `ForwardOpaquePassSystem`, etc.).

4. **Feature Services Layer**
   - `lighting/*`, `shadows/*`, `ibl/*`, `reflection/*`, `post/*`, `2d/*`, `submission/*`
   - reusable domain systems and algorithms.

5. **Resource/Shading Layer**
   - `resources/*`, `materials/*`, shader infrastructure (`Shader*` files)
   - resource creation, material model, shader cache/loader/reflection.

6. **RHI Layer**
   - `rhi/*` + backend implementations (`rhi/OpenGL`, `rhi/Vulkan`, `rhi/diligent`)
   - backend abstraction, command recording, device/resource interfaces.

7. **Policy/Capability Layer**
   - `backend/RenderBackendPolicy.*`
   - backend checks and capability decisions used by rendering logic.

---

## 4) Current Runtime Flow and Connections

## 4.1 Per-frame high-level flow

`RenderPipeline::Render(view)` currently does:

1. prepare frame data (`RenderFrameData`) from scene view,
2. choose mode (`Deferred` / `Forward` / `ForwardPlus`),
3. build/wire graph passes using recipes + executors,
4. execute graph (`RenderGraphOrchestrator` -> `FrameGraph`),
5. run additional overlays and final post stage,
6. leave device/command workflow for present path.

## 4.2 Mode flow (current)

- **Deferred**: GPU Cull -> Shadow -> GBuffer -> Deferred Lighting -> Transparency -> Skybox -> Post (final stage outside recipes).
- **Forward**: GPU Cull -> Shadow -> Forward Opaque -> Transparency -> Skybox -> Post.
- **Forward+**: Shadow -> Forward+ Opaque -> Transparency -> Skybox -> Post.
- **Stereo**: recipe variant + per-eye specialized execution.

## 4.3 Main data and control connections

- `RenderPipeline` -> `renderer/graph/FrameGraphRecipeBuilders.*` to wire pass lists.
- `RenderPipeline` -> `renderer/graph/RenderGraphOrchestrator.*` for compile-if-needed + execute.
- `RenderPipeline` graph executors -> pass systems (`GBufferPassSystem`, `ShadowPassSystem`, etc.).
- pass systems -> lower feature implementations (`LightingPass`, `TransparencyPass`, `ShadowPass`, etc.).
- pass systems + feature code -> `PassResourceBuilders` for descriptor/resource setup.
- all execution eventually binds through `rhi::IDevice` and backend command list implementations.

---

## 5) What Is Making It Messy

## 5.1 Architectural pressure points

1. **RenderPipeline concentration**
   - Too many responsibilities remain in a single orchestrator file.
2. **Hybrid ownership model**
   - Some logic lives in legacy `*Pass` classes, some in `*PassSystem`, some still in `RenderPipeline`.
3. **Cross-layer backend branching**
   - Capability/backend decisions still appear in multiple places.
4. **Global ABI growth risk**
   - `PassPushConstants.h` has become broad and cross-domain.
5. **Naming split confusion**
   - `FrameGraph` core vs `RenderGraph*` orchestrator/recipe naming creates mental overhead.
6. **Editor/runtime overlap risk**
   - editor overlay/ID/outline/debug logic must remain clearly isolated from runtime core flow.

## 5.2 Resulting operational issues

- onboarding becomes slower,
- merge conflicts increase in hot files,
- extension cost rises (new feature touches too many subsystems),
- regressions become easier to introduce during refactors.

---

## 6) Professional Target Design (Unreal-Style Direction)

## 6.1 Target principles

1. **Thin composition root**
   - `RenderPipeline` should be orchestration-only, not heavy pass implementation.
2. **Single pass contract**
   - every pass exposed as:
     - `XxxPassBuildParams`
     - `XxxPassExecuteParams`
     - `BuildParams(...)`
     - `Execute(...)`
3. **Mutation boundary**
   - descriptor/buffer writes in build/setup only,
   - execute stage bind/draw/dispatch only.
4. **Recipe variation, shared implementation**
   - deferred/forward/forward+ differ by recipe composition, not duplicated pass internals.
5. **Backend isolation**
   - policy/capability checks localized in backend/policy and pass internals.
6. **Strict ownership**
   - one graph-facing owner per domain (no split ownership between pipeline and pass).
7. **Documented interface stability**
   - stable ABI contracts for shared buffers/push constants with minimal global scope.

## 6.2 Target logical architecture

- **Layer A: Pipeline Facade**
  - mode selection, frame context creation, top-level render entry.
- **Layer B: Graph Orchestrator**
  - recipe wiring, graph compile scheduling, pass registration.
- **Layer C: Pass Systems**
  - all pass behavior behind explicit Build/Execute contract.
- **Layer D: Resource Builders**
  - per-frame/per-pass/per-draw descriptor and immutable execute payload setup.
- **Layer E: Feature Services**
  - shadows, IBL, probes, lighting grids, post utilities.
- **Layer F: RHI**
  - backend-independent API + backend implementations.

This mirrors Unreal-style separation: renderer orchestration -> render graph -> RHI.

---

## 7) Recommended Future Folder Shape (Goal State)

Use this as target organization policy (can be reached incrementally):

```text
render/
  core/                      # RenderTypes, camera conventions, shared minimal contracts
  graph/                     # FrameGraph core + recipe schema
  orchestrator/              # RenderPipeline facade + graph orchestrator + recipe builders
  passes/
    deferred/
    forward/
    shadows/
    transparency/
    post/
    overlays/                # editor overlays isolated
    streaming/
    common/
  resources/                 # Mesh/Texture/Shader + pass resource builders
  shading/                   # materials + shader cache/loader/reflection
  scene/                     # scene-lighting, clustered/tiled grids, probe/ibl services
  submission/                # draw batching and submission helper contracts
  rhi/                       # RHI + OpenGL/Vulkan/Diligent implementations
```

Note: You do not need a disruptive big-bang move now.  
Use this as **governed destination**, while continuing safe incremental extraction.

---

## 8) Ownership Rules (Mandatory for Clean Scale)

1. A new render feature must declare **single owner system** before implementation.
2. No direct descriptor mutation in execute stage.
3. `RenderPipeline` cannot absorb new pass internals.
4. Backend-specific conditionals must go through policy/capability boundaries.
5. Any new pass must provide explicit `BuildParams` + `Execute` contract.
6. Shared ABI headers should be domain-scoped; avoid monolithic cross-feature ABI growth.
7. Editor-only logic stays in overlay/editor systems; runtime path must remain clean.

---

## 9) Practical End-State Roadmap (Recommended Order)

1. Finish forward extraction sequence:
   - transparent, stereo, forward+ opaque paths into pass systems/contexts.
2. Complete ownership promotion for all major passes:
   - no heavy draw loops left in `RenderPipeline`.
3. Split large shared ABI file (`PassPushConstants.h`) by domain.
4. Introduce pass registry constants (avoid stringly-typed pass names).
5. Keep recipe builders and orchestrator clean, with compile/execute strictly separated.
6. Formalize architecture checks in PR review:
   - ownership, mutation boundary, backend isolation, and naming consistency.

---

## 10) Final Professional Conclusion

The render module is already feature-rich and technically advanced.  
The right next step is **architectural governance**, not random file additions.

The professional “ultimate” goal is:

- thin orchestration,
- strict pass ownership contracts,
- stable dependency directions,
- backend abstraction discipline,
- and a clean folder model that scales with team size and feature count.

If these rules are enforced consistently, the module can reach and sustain an industry-grade, Unreal-style architecture without sacrificing current capabilities.

---

## 11) Ultimate Non-RHI File/Folder Blueprint (Target)

This is the recommended **end-state file structure** excluding backend-specific files under `rhi/OpenGL`, `rhi/Vulkan`, and `rhi/diligent`.

Target size: **89 files** (non-RHI-backend render core).

### 11.1 `render/core/` (7 files)

- `RenderTypes.h` — canonical frame/scene data types (`Drawable`, camera/light/frame payloads).
- `CameraConventions.h` — backend-safe camera/projection conventions and adjustments.
- `RenderContext.h` — lightweight device + command context passed across systems.
- `RenderBackend.h` — backend enum and backend-agnostic identifiers.
- `PipelineSharedResources.h` — cross-pipeline shared caches/resources contract.
- `FrameRenderContext.h` — immutable per-frame execution context.
- `PassABI.h` — shared pass ABI (small/stable push/UBO layouts only).

### 11.2 `render/graph/` (6 files)

- `FrameGraph.h` — render graph API and dependency declarations.
- `FrameGraph.cpp` — graph compile/execute and resource-state scheduling.
- `FrameGraphRecipe.h` — declarative recipe/pass definition schema.
- `FrameGraphRecipe.cpp` — recipe expansion and pass registration helpers.
- `PassRegistry.h` — strongly-typed pass-name registry (avoid string drift).
- `PassRegistry.cpp` — pass-id/name mapping and validation helpers.

### 11.3 `render/orchestrator/` (8 files)

- `RenderPipeline.h` — public render pipeline facade API.
- `RenderPipeline.cpp` — high-level orchestration only (no heavy pass internals).
- `RenderPipelineMode.h` — mode selection contract (`Deferred/Forward/ForwardPlus/Stereo`).
- `RenderGraphOrchestrator.h` — frame graph compile/dirty scheduling interface.
- `RenderGraphOrchestrator.cpp` — graph lifecycle and execute sequencing.
- `FrameGraphRecipeBuilders.h` — mode-specific recipe builder declarations.
- `FrameGraphRecipeBuilders.cpp` — mode recipe composition/wiring.
- `FrameGraphExecutorSet.h` — executor set identifiers and override slots.

### 11.4 `render/passes/common/` (3 files)

- `ForwardSharedEnvironment.h` — shared forward environment/probe resolution helpers.
- `ForwardSharedStateWriter.h` — forward pass shared-state write contract.
- `ForwardSharedStateWriter.cpp` — implementation of forward shared-state writes.

### 11.5 `render/passes/deferred/` (5 files)

- `GBufferPassSystem.h` — deferred GBuffer pass-system API.
- `GBufferPassSystem.cpp` — deferred opaque geometry submission for GBuffer.
- `GBufferPassOpaqueParams.h` — immutable build/execute params for GBuffer opaque path.
- `DeferredLightingPassSystem.h` — deferred lighting pass-system API.
- `DeferredLightingPassSystem.cpp` — deferred light accumulation execution.

### 11.6 `render/passes/forward/` (9 files)

- `ForwardOpaquePassSystem.h` — forward opaque pass-system interface/context.
- `ForwardOpaquePassSystem.cpp` — forward opaque system build/execute wrappers.
- `ForwardOpaqueGeometry.cpp` — shared heavy forward opaque geometry draw loop.
- `TransparencyPassSystem.h` — transparent pass-system API.
- `TransparencyPassSystem.cpp` — sorted transparent draw path and execution.
- `ForwardPlusOpaquePassSystem.h` — forward+ opaque pass-system API.
- `ForwardPlusOpaquePassSystem.cpp` — forward+ batched opaque execution path.
- `ForwardStereoPassSystem.h` — stereo forward pass-system API.
- `ForwardStereoPassSystem.cpp` — stereo-eye forward execution (left/right flow).

### 11.7 `render/passes/shadows/` (3 files)

- `ShadowPassSystem.h` — shadow pass-system API.
- `ShadowPassSystem.cpp` — batched shadow rendering execution.
- `ShadowPassParams.h` — shadow build/execute parameter contracts.

### 11.8 `render/passes/post/` (4 files)

- `SkyboxPassSystem.h` — skybox/procedural sky pass-system API.
- `SkyboxPassSystem.cpp` — skybox execution logic.
- `PostProcessPassSystem.h` — post-process pass-system API.
- `PostProcessPassSystem.cpp` — final post stack execution wrapper.

### 11.9 `render/passes/overlays/` (2 files)

- `EditorOverlayPassSystem.h` — editor overlays API (entity-id/outline/debug).
- `EditorOverlayPassSystem.cpp` — overlay rendering/composition execution.

### 11.10 `render/passes/streaming/` (2 files)

- `StreamingPassSystem.h` — streaming pass API (e.g., NV12 conversion).
- `StreamingPassSystem.cpp` — streaming compute pass implementation.

### 11.11 `render/scene/lighting/` (6 files)

- `SceneLighting.h` — scene-lighting buffer schema and API.
- `SceneLighting.cpp` — scene-lighting buffer fill/update logic.
- `ClusteredLightGrid.h` — clustered-light grid data structures/API.
- `ClusteredLightGrid.cpp` — clustered-light grid build/update implementation.
- `LightCullPassSystem.h` — GPU light-cull pass-system interface.
- `LightCullPassSystem.cpp` — GPU light-cull dispatch/wiring implementation.

### 11.12 `render/scene/culling/` (4 files)

- `GpuCullPassSystem.h` — GPU cull pass-system API.
- `GpuCullPassSystem.cpp` — GPU cull setup/dispatch and draw-list preparation.
- `GpuCullScenePrep.h` — scene-to-GPU-cull upload preparation API.
- `GpuCullScenePrep.cpp` — scene cull prep implementation.

### 11.13 `render/scene/environment/` (4 files)

- `IBL.h` — image-based-lighting service API.
- `IBL.cpp` — IBL generation/binding implementation.
- `ReflectionProbeManager.h` — reflection probe management API.
- `ReflectionProbeManager.cpp` — probe selection/update/capture orchestration.

### 11.14 `render/assets/materials/` (6 files)

- `Material.h` — material definition and parameter model.
- `Material.cpp` — material runtime behavior and bindings.
- `MaterialInstance.h` — per-instance material override model.
- `MaterialInstance.cpp` — material-instance runtime behavior.
- `MaterialBuilder.h` — fluent material creation API.
- `MaterialBuilder.cpp` — material builder implementation.

### 11.15 `render/assets/shaders/` (4 files)

- `ShaderCache.h` — shader cache API and ownership contract.
- `ShaderCache.cpp` — shader cache implementation.
- `ShaderLoader.h` — shader load API from descriptors/assets.
- `ShaderLoader.cpp` — shader loading/compile implementation.

### 11.16 `render/assets/resources/` (4 files)

- `Mesh.h` — GPU mesh resource API.
- `Mesh.cpp` — GPU mesh upload/draw/resource implementation.
- `Texture2D.h` — 2D texture resource API.
- `Texture2D.cpp` — 2D texture load/create/update implementation.

### 11.17 `render/debug/` (2 files)

- `DebugDrawRenderer.h` — debug draw subsystem API.
- `DebugDrawRenderer.cpp` — debug line/text rendering implementation.

### 11.18 `render/ui2d/` (4 files)

- `SpriteRenderer.h` — sprite rendering API.
- `SpriteRenderer.cpp` — sprite rendering implementation.
- `TextRenderer.h` — text rendering API.
- `TextRenderer.cpp` — text rendering implementation.

### 11.19 `render/submission/` (2 files)

- `DrawBatch.h` — draw-batch payload/schema for submission.
- `BatchBuilder.cpp` — batch construction/grouping implementation.

### 11.20 `render/tooling/` (4 files)

- `GpuProfilerRHI.h` — GPU profiling API over RHI.
- `GpuProfilerRHI.cpp` — GPU profiling implementation.
- `PipelineCache.h` — pipeline cache API.
- `PipelineCache.cpp` — pipeline cache implementation.

### 11.21 Why this target is clean

- one folder = one responsibility domain,
- one pass family = one pass owner,
- `RenderPipeline` remains orchestration-focused,
- graph, pass systems, scene services, and assets are clearly separated,
- easy scaling for new features without turning into a single giant file again.

---

## Appendix A — Complete File Inventory (All 297 Files)

## Top level (`EngineSource/eyelib/render/`)

- `BatchRenderer.cpp`
- `BatchRenderer.h`
- `CameraConventions.h`
- `CMakeLists.txt`
- `CullingSystem.cpp`
- `CullingSystem.h`
- `DebugDrawRenderer.cpp`
- `DebugDrawRenderer.h`
- `FullscreenQuad.cpp`
- `FullscreenQuad.h`
- `GpuCullBlockNames.h`
- `GpuProfilerRHI.cpp`
- `GpuProfilerRHI.h`
- `InstancedRenderer.cpp`
- `InstancedRenderer.h`
- `IRenderSceneView.h`
- `LODManager.cpp`
- `LODManager.h`
- `MeshDescriptor.h`
- `MeshDescriptorUtils.cpp`
- `MeshDescriptorUtils.h`
- `PassPushConstants.h`
- `PipelineCache.cpp`
- `PipelineCache.h`
- `RenderBackend.h`
- `Renderer.cpp`
- `Renderer.h`
- `RenderContext.h`
- `RenderTarget.cpp`
- `RenderTarget.h`
- `RenderTypes.h`
- `RENDER_MODULE.md`
- `ShaderCache.cpp`
- `ShaderCache.h`
- `ShaderDescriptor.h`
- `ShaderKey.h`
- `ShaderLoader.cpp`
- `ShaderLoader.h`
- `ShaderPathResolve.h`
- `ShaderReflection.cpp`
- `ShaderReflection.h`
- `ShaderSourceLoader.cpp`
- `ShaderSourceLoader.h`
- `ShaderTemplateName.h`
- `ShaderWatcher.cpp`
- `ShaderWatcher.h`
- `StreamingHooks.cpp`
- `StreamingHooks.h`
- `TextureDescriptor.h`

## `2d/`

- `Font.cpp`
- `Font.h`
- `Primitives.cpp`
- `Primitives.h`
- `Sprite.cpp`
- `Sprite.h`
- `SpriteRenderer.cpp`
- `SpriteRenderer.h`
- `TextRenderer.cpp`
- `TextRenderer.h`

## `backend/`

- `RenderBackendPolicy.cpp`
- `RenderBackendPolicy.h`

## `framegraph/`

- `FrameGraph.cpp`
- `FrameGraph.h`
- `FrameGraphRecipe.cpp`
- `FrameGraphRecipe.h`

## `ibl/`

- `IBL.cpp`
- `IBL.h`

## `lighting/`

- `ClusteredLightGrid.cpp`
- `ClusteredLightGrid.h`
- `LightCullPass.cpp`
- `LightCullPass.h`
- `SceneLighting.cpp`
- `SceneLighting.h`
- `TiledLightGrid.cpp`

## `materials/`

- `Material.cpp`
- `Material.h`
- `MaterialBuilder.cpp`
- `MaterialBuilder.h`
- `MaterialCompiler.cpp`
- `MaterialCompiler.h`
- `MaterialDescriptor.h`
- `MaterialInstance.cpp`
- `MaterialInstance.h`
- `MaterialShader.cpp`
- `MaterialShader.h`

## `passes/`

- `DeferredLightingPassSystem.cpp`
- `DeferredLightingPassSystem.h`
- `EditorOverlayPassSystem.cpp`
- `EditorOverlayPassSystem.h`
- `ForwardOpaqueGeometry.cpp`
- `ForwardOpaquePassSystem.cpp`
- `ForwardOpaquePassSystem.h`
- `GBufferPass.cpp`
- `GBufferPass.h`
- `GBufferPassOpaqueParams.h`
- `GBufferPassSystem.cpp`
- `GBufferPassSystem.h`
- `GpuCullPass.cpp`
- `GpuCullPass.h`
- `GpuCullPassSystem.cpp`
- `GpuCullPassSystem.h`
- `GpuCullScenePrep.cpp`
- `GpuCullScenePrep.h`
- `LightCullPassSystem.cpp`
- `LightCullPassSystem.h`
- `LightingPass.cpp`
- `LightingPass.h`
- `PostProcessPassSystem.cpp`
- `PostProcessPassSystem.h`
- `ShadowPassParams.h`
- `ShadowPassSystem.cpp`
- `ShadowPassSystem.h`
- `SkyboxPassSystem.cpp`
- `SkyboxPassSystem.h`
- `StreamingPassSystem.cpp`
- `StreamingPassSystem.h`
- `TransparencyPass.cpp`
- `TransparencyPass.h`
- `TransparencyPassSystem.cpp`
- `TransparencyPassSystem.h`

## `passes/common/`

- `ForwardSharedEnvironment.h`
- `ForwardSharedStateWriter.cpp`
- `ForwardSharedStateWriter.h`

## `post/`

- `PostProcessor.cpp`
- `PostProcessor.h`

## `reflection/`

- `ReflectionProbe.cpp`
- `ReflectionProbe.h`
- `ReflectionProbeManager.cpp`
- `ReflectionProbeManager.h`

## `renderer/`

- `FrameRenderContext.h`
- `GBuffer.cpp`
- `GBuffer.h`
- `RenderPipeline.cpp`
- `RenderPipeline.h`
- `RenderPipelineMode.h`

## `renderer/graph/`

- `FrameGraphExecutorSet.h`
- `FrameGraphRecipeBuilders.cpp`
- `FrameGraphRecipeBuilders.h`
- `RenderGraphOrchestrator.cpp`
- `RenderGraphOrchestrator.h`
- `RenderGraphRecipes.cpp`
- `RenderGraphRecipes.h`

## `resources/`

- `Mesh.cpp`
- `Mesh.h`
- `MeshOptimizer.cpp`
- `MeshOptimizer.h`
- `PassResourceBuilders.cpp`
- `PassResourceBuilders.h`
- `Shader.cpp`
- `Shader.h`
- `Texture2D.cpp`
- `Texture2D.h`
- `Texture3D.cpp`
- `Texture3D.h`
- `TextureArray.cpp`
- `TextureArray.h`
- `TextureCompression.h`
- `TextureCube.cpp`
- `TextureCube.h`
- `TextureLoader.cpp`
- `TextureLoader.h`
- `TextureTypes.h`

## `rhi/` (root)

- `CreateDevice.cpp`
- `CreateDevice.h`
- `PipelineAttachmentFormatSets.h`
- `RHI.h`
- `ResourceSetLayoutDesc.cpp`
- `ResourceSetLayoutDesc.h`
- `RHITypes.h`
- `RHIUniformLayout.h`
- `RHIUtils.cpp`
- `RHIUtils.h`

## `rhi/diligent/`

- `BufferDiligent.cpp`
- `BufferDiligent.h`
- `CommandListDiligent.cpp`
- `CommandListDiligent.h`
- `CreateDeviceDiligent.cpp`
- `CreateDeviceDiligent.h`
- `DescriptorSetDiligent.cpp`
- `DescriptorSetDiligent.h`
- `DeviceDiligent.cpp`
- `DeviceDiligent.h`
- `DeviceDiligentImpl.h`
- `DiligentEnums.cpp`
- `DiligentEnums.h`
- `PipelineDiligent.cpp`
- `PipelineDiligent.h`
- `PipelineLayoutDiligent.h`
- `QueryPoolDiligent.cpp`
- `QueryPoolDiligent.h`
- `RenderTargetDiligent.cpp`
- `RenderTargetDiligent.h`
- `ShaderDiligent.cpp`
- `ShaderDiligent.h`
- `TextureDiligent.cpp`
- `TextureDiligent.h`

## `rhi/OpenGL/`

- `BindlessHeapGL.cpp`
- `BindlessHeapGL.h`
- `BufferGL.cpp`
- `BufferGL.h`
- `CommandListGL.cpp`
- `CommandListGL.h`
- `DeferredCommandListGL.cpp`
- `DeferredCommandListGL.h`
- `DescriptorSetGL.cpp`
- `DescriptorSetGL.h`
- `DeviceGL.cpp`
- `DeviceGL.h`
- `FenceGL.h`
- `GLEnums.cpp`
- `GLEnums.h`
- `OpenGLResourceSpaceBindings.cpp`
- `OpenGLResourceSpaceBindings.h`
- `PipelineGL.cpp`
- `PipelineGL.h`
- `PipelineLayoutGL.h`
- `QueryPoolGL.cpp`
- `QueryPoolGL.h`
- `RenderTargetGL.cpp`
- `RenderTargetGL.h`
- `ShaderGL.cpp`
- `ShaderGL.h`
- `TextureGL.cpp`
- `TextureGL.h`

## `rhi/Vulkan/`

- `descriptor_binding_table.md`

## `rhi/Vulkan/barriers/`

- `BarrierTrackerVK.cpp`
- `BarrierTrackerVK.h`
- `ResourceStateVK.h`

## `rhi/Vulkan/commands/`

- `CommandBufferVK.cpp`
- `CommandBufferVK.h`
- `CommandListVK.cpp`
- `CommandListVK.h`
- `CommandPoolVK.cpp`
- `CommandPoolVK.h`

## `rhi/Vulkan/core/`

- `CreateDeviceVulkan.cpp`
- `CreateDeviceVulkan.h`
- `DeviceVK.cpp`
- `DeviceVK.h`
- `DeviceVKPresentation.cpp`
- `FrameContextVK.cpp`
- `FrameContextVK.h`
- `InstanceVK.cpp`
- `InstanceVK.h`
- `PhysicalDeviceVK.cpp`
- `PhysicalDeviceVK.h`
- `QueuesVK.cpp`
- `QueuesVK.h`
- `SwapchainVK.cpp`
- `SwapchainVK.h`

## `rhi/Vulkan/descriptors/`

- `BindlessHeapVK.cpp`
- `BindlessHeapVK.h`
- `DescriptorAllocatorVK.cpp`
- `DescriptorAllocatorVK.h`
- `DescriptorCacheVK.cpp`
- `DescriptorCacheVK.h`
- `DescriptorSetVK.cpp`
- `DescriptorSetVK.h`

## `rhi/Vulkan/memory/`

- `AllocatorVK.cpp`
- `AllocatorVK.h`
- `DeletionQueueVK.cpp`
- `DeletionQueueVK.h`
- `UploadHeapVK.cpp`
- `UploadHeapVK.h`
- `vk_mem_alloc.h`

## `rhi/Vulkan/pipelines/`

- `PipelineLayoutVK.h`
- `PipelineVK.cpp`
- `PipelineVK.h`
- `ShaderVK.cpp`
- `ShaderVK.h`
- `VulkanBindingUtils.cpp`
- `VulkanBindingUtils.h`
- `VulkanGlslToSpirv.cpp`
- `VulkanGlslToSpirv.h`
- `VulkanPipelineHash.cpp`
- `VulkanPipelineHash.h`
- `VulkanResourceSpaces.cpp`
- `VulkanResourceSpaces.h`

## `rhi/Vulkan/resources/`

- `BufferVK.cpp`
- `BufferVK.h`
- `RenderTargetVK.cpp`
- `RenderTargetVK.h`
- `TextureVK.cpp`
- `TextureVK.h`

## `rhi/Vulkan/sync/`

- `FenceVK.cpp`
- `FenceVK.h`
- `QueryPoolVK.cpp`
- `QueryPoolVK.h`

## `rhi/Vulkan/utils/`

- `DebugUtilsVK.cpp`
- `DebugUtilsVK.h`
- `VulkanEnums.h`

## `shadows/`

- `CascadeShadowMap.cpp`
- `CascadeShadowMap.h`
- `ShadowPass.cpp`
- `ShadowPass.h`

## `submission/`

- `BatchBuilder.cpp`
- `DrawBatch.h`

