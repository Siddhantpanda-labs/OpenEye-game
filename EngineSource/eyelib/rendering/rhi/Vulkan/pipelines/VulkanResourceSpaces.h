#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Resource-space **`VkDescriptorSetLayout` / `VkPipelineLayout`** factories. Set indices and bindings follow **`descriptor_binding_table.md`**.
 *  Push constant **ranges** are defined in **`VulkanResourceSpaces.cpp`** (**`kPushConstantSize`**) and must match **`vkCmdPushConstants`** in **`CommandListVK`**. */

namespace vkres {

/** Empty set layout (0 bindings). Placeholder for set indices 1–3 in compute layouts so set 4 = SceneBuffers. */
VkDescriptorSetLayout CreateEmptySetLayout(VkDevice device);

/** Set 0 — Frame: binding 0 Camera UBO, binding 1 Time/global UBO (optional).
 *  @param includeMeshShaderStages When true (`VK_EXT_mesh_shader` on device), bindings include task|mesh visibility. */
VkDescriptorSetLayout CreateFrameSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 1 — ForwardPass: 0 shadow/cascade image, 1 light list SSBO, 2 cluster SSBO. */
VkDescriptorSetLayout CreateForwardPassSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 1 — PostPass: 0 scene color, 1 scene depth, 2 velocity (all combined image sampler). */
VkDescriptorSetLayout CreatePostPassSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 1 — ShadowPass: 0 cascade params UBO (optional). */
VkDescriptorSetLayout CreateShadowPassSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 1 — LightCullPass (compute): 0 ParamsBlock UBO, 1 light list output SSBO. */
VkDescriptorSetLayout CreateLightCullPassSetLayout(VkDevice device);

/** Set 1 — DeferredLightingPass: 0–3 GBuffer images, 4 shadow, 5 scene lighting SSBO, 6 light space buffer, 7–9 IBL. */
VkDescriptorSetLayout CreateDeferredLightingPassSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 2 — Material: 0 material UBO, 1–3 textures (combined image sampler). */
VkDescriptorSetLayout CreateMaterialSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 4 — SceneBuffers: 0 draw list, 1 instance/transform, 2 light buffer, 3 cluster (all SSBO). */
VkDescriptorSetLayout CreateSceneBuffersSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Set 0 — IBLPass: 0 environment cube (combined image sampler). */
VkDescriptorSetLayout CreateIBLPassSetLayout(VkDevice device, bool includeMeshShaderStages);

/** Forward: set0 Frame, set1 ForwardPass, set2 Material, set3 Bindless, set4 SceneBuffers. Push constants 256 bytes; when @p includeMeshPushStages, range is visible to vertex|fragment|task|mesh.
 *  When @p useIndependentSets (descriptor buffer bindless + classic sets), `VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT` is OR'd in. */
VkPipelineLayout CreateForwardPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout forwardPassLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout,
    bool useIndependentSets,
    bool includeMeshPushStages);

/** Post: set0 Frame, set1 PostPass. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreatePostPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout postPassLayout,
    bool includeMeshPushStages);

/** Shadow: set0 Frame, set1 ShadowPass, set2 SceneBuffers. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateShadowPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout shadowPassLayout,
    VkDescriptorSetLayout sceneLayout,
    bool includeMeshPushStages);

/** GBuffer: set0 Frame, set1 Material, set2 Bindless, set3 SceneBuffers. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateGBufferPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout,
    bool useIndependentSets,
    bool includeMeshPushStages);

/** DeferredLighting: set0 Frame, set1 DeferredLightingPass. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateDeferredLightingPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout deferredLightingPassLayout,
    bool includeMeshPushStages);

/** ComputeLightCull: set0 Frame, set1 LightCullPass, set2–3 empty, set4 SceneBuffers. Push constants 256 bytes compute. */
VkPipelineLayout CreateComputeLightCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout lightCullPassLayout,
    VkDescriptorSetLayout sceneLayout,
    VkDescriptorSetLayout emptyLayout);

/** ComputeGpuCull: set0 Frame, set1–3 empty, set4 SceneBuffers. Push constants 256 bytes compute. */
VkPipelineLayout CreateComputeGpuCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout sceneLayout,
    VkDescriptorSetLayout emptyLayout);

/** IBL: set0 IBLPass (env cube). Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateIBLPipelineLayout(VkDevice device, VkDescriptorSetLayout iblPassLayout, bool includeMeshPushStages);

/** Set 0 — OutlinePost: 0 scene color, 1 entity ID (combined image sampler). */
VkDescriptorSetLayout CreateOutlinePostSetLayout(VkDevice device, bool includeMeshShaderStages);
/** OutlinePost: set0 OutlinePost. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateOutlinePostPipelineLayout(VkDevice device, VkDescriptorSetLayout outlinePostLayout, bool includeMeshPushStages);

/** ComposeBootstrap: set0 Frame (two UBOs). Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateComposeBootstrapPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout, bool includeMeshPushStages);

/** Set 1 — RasterForwardData: 0 PerObject SSBO, 1 directional light UBO. */
VkDescriptorSetLayout CreateRasterForwardDataSetLayout(VkDevice device, bool includeMeshShaderStages);
/** RasterForwardMinimal: set0 Frame, set1 RasterForwardData. Push constants 256 bytes (optionally includes mesh push stages). */
VkPipelineLayout CreateRasterForwardMinimalPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout rasterForwardDataLayout, bool includeMeshPushStages);
/** Set 1 — RasterGBufferData: 0 PerObject SSBO, 1 MaterialTable SSBO. */
VkDescriptorSetLayout CreateRasterGBufferDataSetLayout(VkDevice device, bool includeMeshShaderStages);
/** RasterGBuffer: set0 Frame, set1 RasterGBufferData, set2 Bindless textures. Push constants 256 bytes (optionally includes mesh push stages).
 *  When @p useIndependentSets is true, ORs `VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT` when available. */
VkPipelineLayout CreateRasterGBufferPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout rasterGBufferDataLayout, VkDescriptorSetLayout bindlessLayout,
    bool useIndependentSets, bool includeMeshPushStages);

/** Set 1 — RasterDeferredLightingData: 0–3 GBuffer+depth, 4 dir light UBO, 5–6 IBL cubes, 7 BRDF LUT. */
VkDescriptorSetLayout CreateRasterDeferredLightingDataSetLayout(VkDevice device, bool includeMeshShaderStages);
/** RasterDeferredLighting: set0 Frame, set1 RasterDeferredLightingData. Push constants 256 bytes. */
VkPipelineLayout CreateRasterDeferredLightingPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout deferredLightDataLayout, bool includeMeshPushStages);

/** PushOnly: 0 descriptor sets; push constants 128 bytes. Used for BRDF LUT, etc. */
VkPipelineLayout CreatePushOnlyPipelineLayout(VkDevice device);
/** PushOnly with task|mesh|fragment push-constant stages for `VK_EXT_mesh_shader` pipelines. See `mesh_shader_rhi.md`. */
VkPipelineLayout CreatePushOnlyPipelineLayoutWithMeshPushStages(VkDevice device);

/** Return pipeline layout for kind. Legacy and PushOnly return pushOnly layout. */
VkPipelineLayout GetPipelineLayoutForKind(VkPipelineLayout forward, VkPipelineLayout post, VkPipelineLayout shadow,
    VkPipelineLayout gbuffer, VkPipelineLayout deferredLighting,
    VkPipelineLayout computeLightCull, VkPipelineLayout computeGpuCull,
    VkPipelineLayout ibl, VkPipelineLayout outlinePost, VkPipelineLayout composeBootstrap,
    VkPipelineLayout RasterForwardMinimal, VkPipelineLayout rasterGBuffer, VkPipelineLayout rasterDeferredLighting,
    VkPipelineLayout pushOnly,
    PipelineLayoutKind kind);

/** Number of descriptor sets the pipeline layout expects. PushOnly/Legacy = 0. */
uint32_t GetRequiredDescriptorSetCount(PipelineLayoutKind kind);

} // namespace vkres
} // namespace rhi
} // namespace rendering
} // namespace eyelib
