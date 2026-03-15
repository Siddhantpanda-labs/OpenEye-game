#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

/** Resource-space descriptor set layouts and pipeline layouts. Binding conventions: see documentation/vulkan/descriptor_binding_table.md */

namespace vkres {

/** Set 0 — Frame: binding 0 Camera UBO, binding 1 Time/global UBO (optional). */
VkDescriptorSetLayout CreateFrameSetLayout(VkDevice device);

/** Set 1 — ForwardPass: 0 shadow/cascade image, 1 light list SSBO, 2 cluster SSBO. */
VkDescriptorSetLayout CreateForwardPassSetLayout(VkDevice device);

/** Set 1 — PostPass: 0 scene color, 1 scene depth, 2 velocity (all combined image sampler). */
VkDescriptorSetLayout CreatePostPassSetLayout(VkDevice device);

/** Set 1 — ShadowPass: 0 cascade params UBO (optional). */
VkDescriptorSetLayout CreateShadowPassSetLayout(VkDevice device);

/** Set 1 — LightCullPass (compute): 0 ParamsBlock UBO, 1 light list output SSBO. */
VkDescriptorSetLayout CreateLightCullPassSetLayout(VkDevice device);

/** Set 1 — DeferredLightingPass: 0–3 GBuffer images, 4 shadow, 5 scene lighting SSBO, 6 light space buffer, 7–9 IBL. */
VkDescriptorSetLayout CreateDeferredLightingPassSetLayout(VkDevice device);

/** Set 2 — Material: 0 material UBO, 1–3 textures (combined image sampler). */
VkDescriptorSetLayout CreateMaterialSetLayout(VkDevice device);

/** Set 4 — SceneBuffers: 0 draw list, 1 instance/transform, 2 light buffer, 3 cluster (all SSBO). */
VkDescriptorSetLayout CreateSceneBuffersSetLayout(VkDevice device);

/** Set 0 — IBLPass: 0 environment cube (combined image sampler). */
VkDescriptorSetLayout CreateIBLPassSetLayout(VkDevice device);

/** Forward: set0 Frame, set1 ForwardPass, set2 Material, set3 Bindless, set4 SceneBuffers. Push constants 128 bytes vertex|fragment. */
VkPipelineLayout CreateForwardPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout forwardPassLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout);

/** Post: set0 Frame, set1 PostPass. Push constants 128 bytes vertex|fragment. */
VkPipelineLayout CreatePostPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout postPassLayout);

/** Shadow: set0 Frame, set1 ShadowPass, set4 SceneBuffers. Push constants 128 bytes vertex|fragment. */
VkPipelineLayout CreateShadowPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout shadowPassLayout,
    VkDescriptorSetLayout sceneLayout);

/** GBuffer: set0 Frame, set2 Material, set3 Bindless, set4 SceneBuffers. Push constants 128 bytes. */
VkPipelineLayout CreateGBufferPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout);

/** DeferredLighting: set0 Frame, set1 DeferredLightingPass. Push constants 128 bytes. */
VkPipelineLayout CreateDeferredLightingPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout deferredLightingPassLayout);

/** ComputeLightCull: set0 Frame, set1 LightCullPass, set4 SceneBuffers. Push constants 128 bytes compute. */
VkPipelineLayout CreateComputeLightCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout lightCullPassLayout,
    VkDescriptorSetLayout sceneLayout);

/** ComputeGpuCull: set0 Frame, set4 SceneBuffers. Push constants 128 bytes compute. */
VkPipelineLayout CreateComputeGpuCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout sceneLayout);

/** IBL: set0 IBLPass (env cube). Push constants 128 bytes (view, projection). */
VkPipelineLayout CreateIBLPipelineLayout(VkDevice device, VkDescriptorSetLayout iblPassLayout);

/** PushOnly: 0 descriptor sets; push constants 128 bytes. Used for BRDF LUT, etc. */
VkPipelineLayout CreatePushOnlyPipelineLayout(VkDevice device);

/** Return pipeline layout for kind. Legacy and PushOnly return pushOnly layout. */
VkPipelineLayout GetPipelineLayoutForKind(VkPipelineLayout forward, VkPipelineLayout post, VkPipelineLayout shadow,
    VkPipelineLayout gbuffer, VkPipelineLayout deferredLighting,
    VkPipelineLayout computeLightCull, VkPipelineLayout computeGpuCull,
    VkPipelineLayout ibl, VkPipelineLayout pushOnly, PipelineLayoutKind kind);

/** Number of descriptor sets the pipeline layout expects. PushOnly/Legacy = 0. */
uint32_t GetRequiredDescriptorSetCount(PipelineLayoutKind kind);

} // namespace vkres
} // namespace rhi
} // namespace render
} // namespace eyelib
