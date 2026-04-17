#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <cassert>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

#if defined(VK_SHADER_STAGE_MESH_BIT_EXT)

namespace {

struct MeshRenderingFormats {
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
};

static void ApplyExplicitAttachmentFormatsMesh(const PipelineAttachmentFormats& af, MeshRenderingFormats& out);
static void InferAttachmentFormatsFromLayoutKindMesh(PipelineLayoutKind kind, MeshRenderingFormats& out);
static MeshRenderingFormats SelectRenderingFormatsMesh(const PipelineDesc& desc);
static bool ValidatePipelineRenderingCreateInfoAgainstDescMesh(DeviceVK* device, const PipelineDesc& desc,
                                                               const MeshRenderingFormats& sel);

static void ApplyExplicitAttachmentFormatsMesh(const PipelineAttachmentFormats& af, MeshRenderingFormats& out) {
    out.colorFormats.clear();
    out.depthFormat = VK_FORMAT_UNDEFINED;
    out.stencilFormat = VK_FORMAT_UNDEFINED;
    for (RHITextureFormat f : af.color)
        out.colorFormats.push_back(ToVkFormat(f));
    if (af.depthStencil == RHITextureFormat::Depth24Stencil8) {
        out.depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        out.stencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    } else if (af.depthStencil != RHITextureFormat::None) {
        out.depthFormat = ToVkFormat(af.depthStencil);
        out.stencilFormat = VK_FORMAT_UNDEFINED;
    }
}

static void InferAttachmentFormatsFromLayoutKindMesh(PipelineLayoutKind kind, MeshRenderingFormats& out) {
    out.colorFormats.clear();
    out.depthFormat = VK_FORMAT_UNDEFINED;
    out.stencilFormat = VK_FORMAT_UNDEFINED;
    switch (kind) {
    case PipelineLayoutKind::Shadow:
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::Forward:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::GBuffer:
        out.colorFormats = {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT,
        };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::DeferredLighting:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::IBL:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    case PipelineLayoutKind::OutlinePost:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    case PipelineLayoutKind::Post:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    case PipelineLayoutKind::ComputeLightCull:
    case PipelineLayoutKind::ComputeGpuCull:
        break;
    case PipelineLayoutKind::PushOnly:
    default:
        out.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM };
        break;
    }
}

static MeshRenderingFormats SelectRenderingFormatsMesh(const PipelineDesc& desc) {
    MeshRenderingFormats out;
    if (desc.attachmentFormats.has_value())
        ApplyExplicitAttachmentFormatsMesh(*desc.attachmentFormats, out);
    else
        InferAttachmentFormatsFromLayoutKindMesh(desc.layoutKind, out);
    return out;
}

static bool ValidatePipelineRenderingCreateInfoAgainstDescMesh(DeviceVK* device, const PipelineDesc& desc,
                                                                const MeshRenderingFormats& sel) {
    const char* dbg = desc.debugName ? desc.debugName : "(no debugName)";
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(device->GetPhysicalDevice());
    if (phys == VK_NULL_HANDLE) {
        core::Log::ErrorF("CreateVkMeshGraphicsPipeline: no physical device (name=%s)", dbg);
        return false;
    }
    VkPhysicalDeviceProperties props = {};
    vkGetPhysicalDeviceProperties(phys, &props);
    const uint32_t maxCA = props.limits.maxColorAttachments;
    const uint32_t maxFO = props.limits.maxFragmentOutputAttachments;
    const size_t colorCount = sel.colorFormats.size();
    if (colorCount > maxCA) {
        core::Log::ErrorF(
            "CreateVkMeshGraphicsPipeline: color attachment count %zu exceeds maxColorAttachments (%u); name=%s", colorCount,
            maxCA, dbg);
        return false;
    }
    if (colorCount > maxFO) {
        core::Log::ErrorF(
            "CreateVkMeshGraphicsPipeline: color attachment count %zu exceeds maxFragmentOutputAttachments (%u); name=%s",
            colorCount, maxFO, dbg);
        return false;
    }
    for (size_t i = 0; i < colorCount; ++i) {
        if (sel.colorFormats[i] == VK_FORMAT_UNDEFINED) {
            core::Log::ErrorF(
                "CreateVkMeshGraphicsPipeline: color attachment[%zu] is VK_FORMAT_UNDEFINED; name=%s", i, dbg);
            return false;
        }
    }
    return true;
}

} // namespace

void* CreateVkMeshGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache) {
    if (!device || !desc.meshShader || !desc.fragmentShader || desc.vertexShader || !pipelineLayout)
        return nullptr;
    if (!device->SupportsMeshShader()) {
        core::Log::ErrorF("CreateVkMeshGraphicsPipeline: VK_EXT_mesh_shader not available (name=%s)",
                          desc.debugName ? desc.debugName : "?");
        return nullptr;
    }
    if (desc.tessControlShader || desc.tessEvalShader || desc.geometryShader) {
        core::Log::ErrorF("CreateVkMeshGraphicsPipeline: tessellation/geometry shaders cannot combine with mesh path (name=%s)",
                          desc.debugName ? desc.debugName : "?");
        return nullptr;
    }
    if (desc.taskShader && !device->GetCapabilities().supportsMeshTaskShader) {
        core::Log::ErrorF("CreateVkMeshGraphicsPipeline: task shader requested but task stage not supported (name=%s)",
                          desc.debugName ? desc.debugName : "?");
        return nullptr;
    }

    ShaderVK* ts = desc.taskShader ? static_cast<ShaderVK*>(desc.taskShader) : nullptr;
    ShaderVK* ms = static_cast<ShaderVK*>(desc.meshShader);
    ShaderVK* fs = static_cast<ShaderVK*>(desc.fragmentShader);
    if (!ms->m_module || !fs->m_module) return nullptr;
    if (ts && !ts->m_module) return nullptr;

    const MeshRenderingFormats selectedFormats = SelectRenderingFormatsMesh(desc);
    if (!ValidatePipelineRenderingCreateInfoAgainstDescMesh(device, desc, selectedFormats))
        return nullptr;

    const char* dbg = desc.debugName ? desc.debugName : "(no debugName)";
    DepthStencilState depthState = desc.depthStencil;
    if (selectedFormats.depthFormat == VK_FORMAT_UNDEFINED) {
        if (depthState.depthTest || depthState.depthWrite) {
            static std::atomic<int> s_depthClampWarns{0};
            const int n = s_depthClampWarns.fetch_add(1);
            if (n < 32) {
                core::Log::WarnF(
                    "CreateVkMeshGraphicsPipeline: depth test/write but no depth attachment in VkPipelineRenderingCreateInfo; "
                    "clamping off. name=%s",
                    dbg);
            }
            depthState.depthTest = false;
            depthState.depthWrite = false;
        }
    }
    if (selectedFormats.stencilFormat == VK_FORMAT_UNDEFINED && depthState.stencilTest) {
        static std::atomic<int> s_stencilClampWarns{0};
        const int n = s_stencilClampWarns.fetch_add(1);
        if (n < 16) {
            core::Log::WarnF(
                "CreateVkMeshGraphicsPipeline: stencil test but no stencilAttachmentFormat; clamping off. name=%s", dbg);
        }
        depthState.stencilTest = false;
    }

    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pipelineLayout);
    VkPipelineCache cache = static_cast<VkPipelineCache>(pipelineCache);

    const char* entryName = "main";
    VkPipelineShaderStageCreateInfo stages[3] = {};
    uint32_t stageCount = 0;
    if (ts) {
        stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stageCount].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
        stages[stageCount].module = static_cast<VkShaderModule>(ts->m_module);
        stages[stageCount].pName = entryName;
        ++stageCount;
    }
    stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[stageCount].module = static_cast<VkShaderModule>(ms->m_module);
    stages[stageCount].pName = entryName;
    ++stageCount;
    stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = static_cast<VkShaderModule>(fs->m_module);
    stages[stageCount].pName = entryName;
    ++stageCount;

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = desc.raster.cullEnable ? (desc.raster.frontCounterClockwise ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT)
                                             : VK_CULL_MODE_NONE;
    raster.frontFace = desc.raster.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = desc.raster.depthBiasEnable ? VK_TRUE : VK_FALSE;
    raster.depthBiasConstantFactor = desc.raster.depthBiasConstant;
    raster.depthBiasSlopeFactor = desc.raster.depthBiasSlope;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    const uint32_t colorAttachmentCount = static_cast<uint32_t>(selectedFormats.colorFormats.size());
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(colorAttachmentCount);
    for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
        VkPipelineColorBlendAttachmentState& a = colorBlendAttachments[i];
        a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        a.blendEnable = desc.blend.blendEnable ? VK_TRUE : VK_FALSE;
        a.srcColorBlendFactor = ToVkBlendFactor(desc.blend.srcColor);
        a.dstColorBlendFactor = ToVkBlendFactor(desc.blend.dstColor);
        a.colorBlendOp = ToVkBlendOp(desc.blend.colorOp);
        a.srcAlphaBlendFactor = ToVkBlendFactor(desc.blend.srcAlpha);
        a.dstAlphaBlendFactor = ToVkBlendFactor(desc.blend.dstAlpha);
        a.alphaBlendOp = ToVkBlendOp(desc.blend.alphaOp);
    }

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = colorAttachmentCount;
    colorBlend.pAttachments = colorAttachmentCount ? colorBlendAttachments.data() : nullptr;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthState.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthState.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ToVkCompareOp(depthState.depthCompare);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = depthState.stencilTest ? VK_TRUE : VK_FALSE;
    depthStencil.front = depthStencil.back = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 };

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = colorAttachmentCount;
    renderingInfo.pColorAttachmentFormats = colorAttachmentCount ? selectedFormats.colorFormats.data() : nullptr;
    renderingInfo.depthAttachmentFormat = selectedFormats.depthFormat;
    renderingInfo.stencilAttachmentFormat = selectedFormats.stencilFormat;
    assert(colorBlend.attachmentCount == renderingInfo.colorAttachmentCount);

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    if (device->SupportsExtendedDynamicState()) {
        dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
    }
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo gpInfo = {};
    gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpInfo.pNext = &renderingInfo;
    gpInfo.stageCount = stageCount;
    gpInfo.pStages = stages;
    gpInfo.pVertexInputState = &vertexInput;
    gpInfo.pInputAssemblyState = &inputAssembly;
    gpInfo.pViewportState = &viewportState;
    gpInfo.pRasterizationState = &raster;
    gpInfo.pMultisampleState = &multisample;
    gpInfo.pColorBlendState = &colorBlend;
    gpInfo.pDepthStencilState = &depthStencil;
    gpInfo.pDynamicState = &dynamicState;
    gpInfo.layout = layout;
    gpInfo.renderPass = VK_NULL_HANDLE;
    gpInfo.subpass = 0;
    gpInfo.basePipelineHandle = VK_NULL_HANDLE;
    gpInfo.basePipelineIndex = -1;
#if defined(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (device->SupportsDescriptorBuffer())
        gpInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines(vkDevice, cache, 1, &gpInfo, nullptr, &pipeline);
    if (res != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
        core::Log::ErrorF(
            "vkCreateGraphicsPipelines (mesh) failed: VkResult=%d (0x%08X) layoutKind=%u colorAttachments=%u name=%s",
            static_cast<int>(res), static_cast<unsigned int>(static_cast<uint32_t>(res)),
            static_cast<unsigned>(static_cast<uint32_t>(desc.layoutKind)), colorAttachmentCount, dbg);
        return nullptr;
    }
    device->DeveloperLogPipelineExecutablePropertiesOnce(pipeline, desc.debugName);
    return pipeline;
}

#else

void* CreateVkMeshGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache) {
    (void)device;
    (void)desc;
    (void)pipelineLayout;
    (void)pipelineCache;
    return nullptr;
}

#endif

} // namespace rhi
} // namespace rendering
} // namespace eyelib
