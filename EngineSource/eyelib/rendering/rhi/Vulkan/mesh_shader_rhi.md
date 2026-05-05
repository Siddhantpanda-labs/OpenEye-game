# Mesh shading (Vulkan RHI)

Companion to `documentation/vulkan/steps_todo.md` **Phase 9**. This engine enables **`VK_EXT_mesh_shader`** (cross-vendor path), not **`VK_NV_mesh_shader`** alone.

---

## 1. Device and caps

- Extension: **`VK_EXT_mesh_shader`** when advertised and `VkPhysicalDeviceMeshShaderFeaturesEXT::meshShader` is supported.
- Optional **task** stage: enabled only when `taskShader` is supported on the device (`RHICapabilities::supportsMeshTaskShader`).
- Limits mirrored in **`RHICapabilities`**: `meshShaderMaxOutputVertices`, `meshShaderMaxOutputPrimitives`, `meshShaderMaxPayloadAndSharedMemoryBytes`.
- Entry points: **`vkCmdDrawMeshTasksEXT`**, **`vkCmdDrawMeshTasksIndirectEXT`** (loaded after `vkCreateDevice`). If `vkCmdDrawMeshTasksEXT` fails to load, mesh shading is treated as **off**.

---

## 2. Pipelines (`PipelineDesc`)

| Field | Classic graphics | Mesh graphics |
|-------|------------------|---------------|
| `vertexShader` | Required | Must be **null** |
| `meshShader` | null | **Required** (`ShaderStage::Mesh` SPIR-V module) |
| `taskShader` | null | Optional (`ShaderStage::Task`) |
| `fragmentShader` | Required | Required |
| `tess*`, `geometryShader` | Optional | **Must be null** (incompatible) |

Creation goes through **`CreateVkGraphicsPipeline`** → **`CreateVkMeshGraphicsPipeline`** (`PipelineVKMesh.cpp`) when `meshShader` is set.

- **Input assembly:** `VK_PRIMITIVE_TOPOLOGY_POINT_LIST` (required for mesh-only pipelines).
- **Vertex input:** empty (no vertex attributes).
- **Dynamic rendering / blend / depth:** same contract as classic pipelines (`dynamic_rendering_pass_contract.md`).
- **Graphics pipeline library:** **not** used on the mesh path (`pNext` is only `VkPipelineRenderingCreateInfo`).

---

## 3. Pipeline layouts (resource-space + `PushOnly`)

Mesh pipelines use push constants on **task | mesh | fragment** (layout includes **task** stage flags even when the SPIR-V has no task shader, so one layout fits both).

**Resource-space passes (Forward, GBuffer, Post, Shadow, DeferredLighting, IBL, OutlinePost):** when **`VK_EXT_mesh_shader`** is enabled on the device, the same **`VkPipelineLayout`** as vertex pipelines is built with a **single push-constant range** whose **`stageFlags`** are **vertex | fragment | task | mesh** (same offset/size as before). Descriptor set bindings in **`VulkanResourceSpaces`** gain **task | mesh** visibility where passes use **`kGraphicsStages`**, and **Frame / SceneBuffers** extend **`kAllStages`** the same way. **Bindless** set layout in **`DeviceVK`** also ORs mesh stages when mesh is supported. Classic vertex pipelines are unchanged at the API level; **`vkCmdPushConstants`** continues to pass a **subset** of the layout’s stages (**vertex | fragment**).

**`PushOnly` + mesh (zero sets):** a dedicated **`VkPipelineLayout`** with **no** descriptor sets and push range **task | mesh | fragment** remains at **`DeviceVK::GetMeshPushPipelineLayout()`** for tests and minimal draws without resource sets.

**`PipelineVK`:** mesh + **Forward** / **GBuffer** / … uses **`GetPipelineLayoutForKind`** for **`vkCreateGraphicsPipelines`** and stores **`m_nativePipelineLayout`** plus **`m_requiredSetCount`** like classic graphics. **PushOnly** mesh keeps **`m_nativePipelineLayout == nullptr`**; **`CommandListVK::PushGraphicsConstantsIfPossible`** falls back to **`GetMeshPushPipelineLayout()`** for that case.

---

## 4. Shaders and SPIR-V

- **`ShaderStage::Task`** / **`ShaderStage::Mesh`** for **`CreateShader`** / **`CreateShaderFromBinary`**.
- GLSL → SPIR-V: **`VulkanCompileGlslToSpirv`** uses **shaderc** `shaderc_glsl_task_shader` / `shaderc_glsl_mesh_shader` (requires a recent Vulkan SDK / shaderc).
- Asset naming (under `assets/shaders/rendering/vulkan/` for EyelibRendering): **`.task`**, **`.mesh`**, plus **`.vert` / `.frag`**. **`DeviceVK::ResolveShaderPath`** maps stages to these extensions.

---

## 5. Recording draws

- **`ICommandList::DrawMeshTasks`** → `vkCmdDrawMeshTasksEXT` when **`UsesMeshShaders()`** on the bound pipeline and **`SupportsMeshShader()`** on the device.
- **`DrawMeshTasksIndirect`**: buffer layout matches **`VkDrawMeshTasksIndirectCommandEXT`** (three `uint32_t` group counts per draw; default stride **12** bytes).

Push constants for mesh pipelines: **`CommandListVK::PushGraphicsConstantsIfPossible`** uses **task | mesh | fragment** stage flags when the bound pipeline **`UsesMeshShaders()`**.

---

## 6. Policy helper

**`eyelib::rendering::backend::SupportsMeshShader(device)`** mirrors **`RHICapabilities::supportsMeshShader`**.
