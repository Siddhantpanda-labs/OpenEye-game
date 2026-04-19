#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanResourceSpaces.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {
namespace vkres {

static constexpr VkShaderStageFlags kAllStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
static constexpr VkShaderStageFlags kGraphicsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
static constexpr uint32_t kPushConstantSize = 256;

#if defined(VK_SHADER_STAGE_MESH_BIT_EXT) && defined(VK_SHADER_STAGE_TASK_BIT_EXT)
static constexpr VkShaderStageFlags kMeshAmpStages =
    VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
#else
static constexpr VkShaderStageFlags kMeshAmpStages = 0u;
#endif

/** Descriptor visibility for graphics sets when mesh shading may read the same bindings as vertex shaders. */
static VkShaderStageFlags GraphicsBindingStages(bool includeMesh) {
    VkShaderStageFlags f = kGraphicsStages;
    if (includeMesh && kMeshAmpStages != 0u)
        f |= kMeshAmpStages;
    return f;
}

/** Frame + SceneBuffers use `kAllStages`; extend with mesh|task when mesh pipelines share those sets. */
static VkShaderStageFlags AllBindingStages(bool includeMesh) {
    VkShaderStageFlags f = kAllStages;
    if (includeMesh && kMeshAmpStages != 0u)
        f |= kMeshAmpStages;
    return f;
}

/** Single push-constant range (offset 0, size `kPushConstantSize`) for resource-space graphics layouts: union of stages used by vertex or mesh pipelines in that pass. */
static VkShaderStageFlags GraphicsPushStagesForPipelineLayout(bool includeMesh) {
    return GraphicsBindingStages(includeMesh);
}

/** Empty descriptor set layout (0 bindings). Used as placeholder for set indices 1–3 in compute layouts so set 4 = SceneBuffers. */
VkDescriptorSetLayout CreateEmptySetLayout(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 0;
    info.pBindings = nullptr;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateEmptySetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateFrameSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = AllBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = st;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateFrameSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateForwardPassSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[6] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = st;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = st;
    for (uint32_t i = 3; i < 6; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 6;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateForwardPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreatePostPassSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    // PostPass: 0=scene color, 1=depth, 2=velocity; 3-6 for SSR (albedo, PBR, scene tex, skybox)
    VkDescriptorSetLayoutBinding bindings[7] = {};
    for (uint32_t i = 0; i < 7; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 7;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreatePostPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateShadowPassSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateShadowPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateLightCullPassSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateLightCullPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateDeferredLightingPassSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[10] = {};
    for (uint32_t i = 0; i < 4; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = st;
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = st;
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = st;
    for (uint32_t i = 7; i < 10; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 10;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateDeferredLightingPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateMaterialSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;
    for (uint32_t i = 1; i < 4; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 4;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateMaterialSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateSceneBuffersSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = AllBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[4] = {};
    for (uint32_t i = 0; i < 4; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = st;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 4;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateSceneBuffersSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkDescriptorSetLayout CreateIBLPassSetLayout(VkDevice device, bool includeMeshShaderStages) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = GraphicsBindingStages(includeMeshShaderStages);

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateIBLPassSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

static VkPipelineLayout createPipelineLayoutCommon(VkDevice device,
    const VkDescriptorSetLayout* setLayouts, uint32_t setLayoutCount,
    VkShaderStageFlags pushStages,
    VkPipelineLayoutCreateFlags extraFlags) {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = pushStages;
    pushRange.offset = 0;
    pushRange.size = kPushConstantSize;

    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.flags = extraFlags;
    info.setLayoutCount = setLayoutCount;
    info.pSetLayouts = setLayouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreatePipelineLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: createPipelineLayoutCommon failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipelineLayout CreateForwardPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout forwardPassLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout,
    bool useIndependentSets,
    bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[5] = { frameLayout, forwardPassLayout, materialLayout, bindlessLayout, sceneLayout };
#if defined(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT)
    const VkPipelineLayoutCreateFlags flags =
        useIndependentSets ? VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT : 0u;
#else
    (void)useIndependentSets;
    const VkPipelineLayoutCreateFlags flags = 0u;
#endif
    return createPipelineLayoutCommon(device, layouts, 5, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), flags);
}

VkPipelineLayout CreatePostPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout postPassLayout,
    bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[2] = { frameLayout, postPassLayout };
    return createPipelineLayoutCommon(device, layouts, 2, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkPipelineLayout CreateShadowPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout shadowPassLayout,
    VkDescriptorSetLayout sceneLayout,
    bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[3] = { frameLayout, shadowPassLayout, sceneLayout };
    return createPipelineLayoutCommon(device, layouts, 3, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkPipelineLayout CreateGBufferPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorSetLayout bindlessLayout,
    VkDescriptorSetLayout sceneLayout,
    bool useIndependentSets,
    bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[4] = { frameLayout, materialLayout, bindlessLayout, sceneLayout };
#if defined(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT)
    const VkPipelineLayoutCreateFlags flags =
        useIndependentSets ? VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT : 0u;
#else
    (void)useIndependentSets;
    const VkPipelineLayoutCreateFlags flags = 0u;
#endif
    return createPipelineLayoutCommon(device, layouts, 4, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), flags);
}

VkPipelineLayout CreateDeferredLightingPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout deferredLightingPassLayout,
    bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[2] = { frameLayout, deferredLightingPassLayout };
    return createPipelineLayoutCommon(device, layouts, 2, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkPipelineLayout CreateComputeLightCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout lightCullPassLayout,
    VkDescriptorSetLayout sceneLayout,
    VkDescriptorSetLayout emptyLayout) {
    VkDescriptorSetLayout layouts[5] = { frameLayout, lightCullPassLayout, emptyLayout, emptyLayout, sceneLayout };
    return createPipelineLayoutCommon(device, layouts, 5, VK_SHADER_STAGE_COMPUTE_BIT, 0u);
}

VkPipelineLayout CreateComputeGpuCullPipelineLayout(VkDevice device,
    VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout sceneLayout,
    VkDescriptorSetLayout emptyLayout) {
    VkDescriptorSetLayout layouts[5] = { frameLayout, emptyLayout, emptyLayout, emptyLayout, sceneLayout };
    return createPipelineLayoutCommon(device, layouts, 5, VK_SHADER_STAGE_COMPUTE_BIT, 0u);
}

VkPipelineLayout CreateIBLPipelineLayout(VkDevice device, VkDescriptorSetLayout iblPassLayout, bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[1] = { iblPassLayout };
    return createPipelineLayoutCommon(device, layouts, 1, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkDescriptorSetLayout CreateOutlinePostSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = st;
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateOutlinePostSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipelineLayout CreateOutlinePostPipelineLayout(VkDevice device, VkDescriptorSetLayout outlinePostLayout, bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[1] = { outlinePostLayout };
    return createPipelineLayoutCommon(device, layouts, 1, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkPipelineLayout CreateComposeBootstrapPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout, bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[1] = { frameLayout };
    return createPipelineLayoutCommon(device, layouts, 1, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkDescriptorSetLayout CreateRasterForwardM5DataSetLayout(VkDevice device, bool includeMeshShaderStages) {
    const VkShaderStageFlags st = GraphicsBindingStages(includeMeshShaderStages);
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = st;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = st;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 2;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreateRasterForwardM5DataSetLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipelineLayout CreateRasterForwardM5MinimalPipelineLayout(VkDevice device, VkDescriptorSetLayout frameLayout,
    VkDescriptorSetLayout rasterM5DataLayout, bool includeMeshPushStages) {
    VkDescriptorSetLayout layouts[2] = { frameLayout, rasterM5DataLayout };
    return createPipelineLayoutCommon(device, layouts, 2, GraphicsPushStagesForPipelineLayout(includeMeshPushStages), 0u);
}

VkPipelineLayout CreatePushOnlyPipelineLayout(VkDevice device) {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = kGraphicsStages;
    pushRange.offset = 0;
    pushRange.size = kPushConstantSize;
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreatePipelineLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreatePushOnlyPipelineLayout failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
}

VkPipelineLayout CreatePushOnlyPipelineLayoutWithMeshPushStages(VkDevice device) {
#if defined(VK_SHADER_STAGE_MESH_BIT_EXT) && defined(VK_SHADER_STAGE_TASK_BIT_EXT)
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags =
        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = kPushConstantSize;
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult res = vkCreatePipelineLayout(device, &info, nullptr, &layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("VulkanResourceSpaces: CreatePushOnlyPipelineLayoutWithMeshPushStages failed (0x%x)", res);
        return VK_NULL_HANDLE;
    }
    return layout;
#else
    (void)device;
    return VK_NULL_HANDLE;
#endif
}

VkPipelineLayout GetPipelineLayoutForKind(VkPipelineLayout forward, VkPipelineLayout post, VkPipelineLayout shadow,
    VkPipelineLayout gbuffer, VkPipelineLayout deferredLighting,
    VkPipelineLayout computeLightCull, VkPipelineLayout computeGpuCull,
    VkPipelineLayout ibl, VkPipelineLayout outlinePost, VkPipelineLayout composeBootstrap,
    VkPipelineLayout rasterForwardM5Minimal, VkPipelineLayout pushOnly,
    PipelineLayoutKind kind) {
    switch (kind) {
        case PipelineLayoutKind::Forward: return forward;
        case PipelineLayoutKind::Post: return post;
        case PipelineLayoutKind::Shadow: return shadow;
        case PipelineLayoutKind::GBuffer: return gbuffer;
        case PipelineLayoutKind::DeferredLighting: return deferredLighting;
        case PipelineLayoutKind::ComputeLightCull: return computeLightCull;
        case PipelineLayoutKind::ComputeGpuCull: return computeGpuCull;
        case PipelineLayoutKind::IBL: return ibl;
        case PipelineLayoutKind::OutlinePost: return outlinePost;
        case PipelineLayoutKind::ComposeBootstrap: return composeBootstrap;
        case PipelineLayoutKind::RasterForwardM5Minimal: return rasterForwardM5Minimal;
        case PipelineLayoutKind::PushOnly: return pushOnly; /* Legacy aliases PushOnly (same enum value) */
    }
    return VK_NULL_HANDLE;
}

uint32_t GetRequiredDescriptorSetCount(PipelineLayoutKind kind) {
    switch (kind) {
        case PipelineLayoutKind::Forward: return 5;
        case PipelineLayoutKind::Post: return 2;
        case PipelineLayoutKind::Shadow: return 3;
        case PipelineLayoutKind::GBuffer: return 4;
        case PipelineLayoutKind::DeferredLighting: return 2;
        case PipelineLayoutKind::ComputeLightCull: return 5;
        case PipelineLayoutKind::ComputeGpuCull: return 5;
        case PipelineLayoutKind::IBL: return 1;
        case PipelineLayoutKind::OutlinePost: return 1;
        case PipelineLayoutKind::ComposeBootstrap: return 1;
        case PipelineLayoutKind::RasterForwardM5Minimal: return 2;
        case PipelineLayoutKind::PushOnly: return 0; /* Legacy aliases PushOnly (same enum value) */
    }
    return 0;
}

} // namespace vkres
} // namespace rhi
} // namespace rendering
} // namespace eyelib
