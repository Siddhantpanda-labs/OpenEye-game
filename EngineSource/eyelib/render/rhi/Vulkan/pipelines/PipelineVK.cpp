#include "eyelib/render/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/VulkanResourceSpaces.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/pipelines/VulkanPipelineHash.h"
#include "eyelib/render/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/render/ShaderPathResolve.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace eyelib {
namespace render {
namespace rhi {

PipelineVK::PipelineVK(DeviceVK* device) : m_device(device) {}

PipelineVK::~PipelineVK() {
    if (m_pipeline && m_device && m_device->GetDevice()) {
        vkDestroyPipeline(static_cast<VkDevice>(m_device->GetDevice()), static_cast<VkPipeline>(m_pipeline), nullptr);
        m_pipeline = nullptr;
    }
}

void PipelineVK::GetComputeBlockPresence(ComputeBlockPresence& out) const { out = m_computeBlockPresence; }

namespace {

struct RenderingFormatSelection {
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
};

static RenderingFormatSelection SelectRenderingFormats(const PipelineDesc& desc) {
    RenderingFormatSelection out;

    // Conservative default for legacy/test paths.
    out.colorFormats = { VK_FORMAT_B8G8R8A8_UNORM };

    switch (desc.layoutKind) {
    case PipelineLayoutKind::Shadow:
        out.colorFormats.clear();
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::Forward:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::GBuffer:
        out.colorFormats = {
            VK_FORMAT_R16G16B16A16_SFLOAT, // position
            VK_FORMAT_R16G16B16A16_SFLOAT, // normal
            VK_FORMAT_R8G8B8A8_UNORM,      // albedo
            VK_FORMAT_R8G8B8A8_UNORM,      // pbr params
            VK_FORMAT_R16G16B16A16_SFLOAT  // velocity
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
    case PipelineLayoutKind::Post:
        out.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM };
        break;
    case PipelineLayoutKind::OutlinePost:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    default:
        break;
    }

    // Shader-name specialization for post passes that target non-default offscreen formats.
    if (desc.debugName) {
        const std::string shaderName(desc.debugName);
        if (desc.layoutKind == PipelineLayoutKind::Post) {
            if (shaderName == "passes/post/bloom_extract" || shaderName == "passes/post/bloom_blur" ||
                shaderName == "passes/post/ssr" || shaderName == "passes/post/depth_pyramid")
                out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
            else if (shaderName == "passes/post/ssao" || shaderName == "passes/post/ssao_blur")
                out.colorFormats = { VK_FORMAT_R8_UNORM };
            else if (shaderName == "passes/post/copy")
                out.colorFormats = { VK_FORMAT_B8G8R8A8_UNORM };
            else if (shaderName == "passes/sky/skybox" || shaderName == "passes/sky/skybox_procedural") {
                out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
                out.depthFormat = VK_FORMAT_D32_SFLOAT;
            }
        }
        if (shaderName == "debug/lines") {
            out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
            out.depthFormat = VK_FORMAT_D32_SFLOAT;
        }
        if (shaderName == "passes/ibl/irradiance_convolution" || shaderName == "passes/ibl/prefilter")
            out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    }

    return out;
}

} // namespace

void* CreateVkGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache) {
    if (!device || !desc.vertexShader || !desc.fragmentShader || !pipelineLayout || !pipelineCache)
        return nullptr;
    ShaderVK* vs = static_cast<ShaderVK*>(desc.vertexShader);
    ShaderVK* fs = static_cast<ShaderVK*>(desc.fragmentShader);
    if (!vs->m_module || !fs->m_module) return nullptr;

    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pipelineLayout);
    VkPipelineCache cache = static_cast<VkPipelineCache>(pipelineCache);

    const char* entryName = "main";
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = static_cast<VkShaderModule>(vs->m_module);
    stages[0].pName = entryName;
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = static_cast<VkShaderModule>(fs->m_module);
    stages[1].pName = entryName;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    if (!desc.vertexLayout.attributes.empty() && desc.vertexLayout.strideInBytes > 0) {
        VkVertexInputBindingDescription binding = {};
        binding.binding = 0;
        binding.stride = desc.vertexLayout.strideInBytes;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(binding);
        for (const auto& a : desc.vertexLayout.attributes) {
            VkVertexInputAttributeDescription attr = {};
            attr.location = a.location;
            attr.binding = 0;
            attr.format = ToVkVertexFormat(a.type);
            attr.offset = a.offsetInBytes;
            attrs.push_back(attr);
        }
    }
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = ToVkPrimitiveTopology(desc.topology);
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
    raster.cullMode = desc.raster.cullEnable ? (desc.raster.frontCounterClockwise ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT) : VK_CULL_MODE_NONE;
    raster.frontFace = desc.raster.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = desc.raster.depthBiasEnable ? VK_TRUE : VK_FALSE;
    raster.depthBiasConstantFactor = desc.raster.depthBiasConstant;
    raster.depthBiasSlopeFactor = desc.raster.depthBiasSlope;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = desc.blend.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = ToVkBlendFactor(desc.blend.srcColor);
    colorBlendAttachment.dstColorBlendFactor = ToVkBlendFactor(desc.blend.dstColor);
    colorBlendAttachment.colorBlendOp = ToVkBlendOp(desc.blend.colorOp);
    colorBlendAttachment.srcAlphaBlendFactor = ToVkBlendFactor(desc.blend.srcAlpha);
    colorBlendAttachment.dstAlphaBlendFactor = ToVkBlendFactor(desc.blend.dstAlpha);
    colorBlendAttachment.alphaBlendOp = ToVkBlendOp(desc.blend.alphaOp);

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencil.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencil.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ToVkCompareOp(desc.depthStencil.depthCompare);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = desc.depthStencil.stencilTest ? VK_TRUE : VK_FALSE;
    depthStencil.front = depthStencil.back = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 };

    const RenderingFormatSelection selectedFormats = SelectRenderingFormats(desc);
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(selectedFormats.colorFormats.size());
    renderingInfo.pColorAttachmentFormats = selectedFormats.colorFormats.empty() ? nullptr : selectedFormats.colorFormats.data();
    renderingInfo.depthAttachmentFormat = selectedFormats.depthFormat;
    renderingInfo.stencilAttachmentFormat = selectedFormats.stencilFormat;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo gpInfo = {};
    gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpInfo.pNext = &renderingInfo;
    gpInfo.stageCount = 2;
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

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines(vkDevice, cache, 1, &gpInfo, nullptr, &pipeline);
    return (res == VK_SUCCESS && pipeline) ? pipeline : nullptr;
}

void* CreateVkComputePipeline(DeviceVK* device, void* pipelineLayout, void* pipelineCache, void* shaderModule) {
    if (!device || !pipelineLayout || !pipelineCache || !shaderModule) return nullptr;
    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pipelineLayout);
    VkPipelineCache cache = static_cast<VkPipelineCache>(pipelineCache);
    VkShaderModule module = static_cast<VkShaderModule>(shaderModule);
    if (vkDevice == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || cache == VK_NULL_HANDLE || module == VK_NULL_HANDLE)
        return nullptr;

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = module;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage = stageInfo;
    createInfo.layout = layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateComputePipelines(vkDevice, cache, 1, &createInfo, nullptr, &pipeline);
    return (res == VK_SUCCESS && pipeline) ? pipeline : nullptr;
}

IPipeline* CreateGraphicsPipelineVK(DeviceVK* device, const PipelineDesc& desc) {
    if (!device) return nullptr;
    const uint64_t hash = HashPipelineDesc(desc);
    void* pipeline = device->GetCachedGraphicsPipeline(hash);
    void* layoutToUse = nullptr;
    if (desc.layoutKind != PipelineLayoutKind::Legacy) {
        layoutToUse = device->GetPipelineLayoutForKind(desc.layoutKind);
    }
    if (!layoutToUse) {
        layoutToUse = device->GetDefaultPipelineLayout();
    }
    if (!pipeline && desc.vertexShader && desc.fragmentShader && layoutToUse && device->GetPipelineCache()) {
        void* created = CreateVkGraphicsPipeline(device, desc, layoutToUse, device->GetPipelineCache());
        if (created) {
            if (desc.debugName)
                SetDebugNamePipeline(device, created, desc.debugName);
            device->RegisterGraphicsPipeline(hash, created);
            pipeline = created;
        }
    }
    auto* p = new PipelineVK(device);
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_patchVertexCount = desc.patchVertexCount;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    p->m_pipeline = pipeline;
    p->m_layoutKind = desc.layoutKind;
    p->m_nativePipelineLayout = (desc.layoutKind != PipelineLayoutKind::Legacy && layoutToUse) ? layoutToUse : nullptr;
    p->m_requiredSetCount = static_cast<uint8_t>(vkres::GetRequiredDescriptorSetCount(desc.layoutKind));
    return p;
}

IPipeline* CreateGraphicsPipelineFromNameVK(DeviceVK* device, const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology, const DepthStencilState* depthStencilOverride, PipelineLayoutKind layoutKind) {
    if (!device || !shaderName || !shaderName[0]) return nullptr;
    const std::string logicalName = ShaderPathToLogicalName(shaderName);
    if (logicalName.empty()) return nullptr;
    std::string root = platform::filesystem::GetEngineAssetsRoot();
    std::string baseDir = root.empty()
        ? std::string("assets/shaders/vulkan/")
        : platform::filesystem::NormalizePath(root + "/assets/shaders/vulkan/");
    std::string vertPath = baseDir + logicalName + ".vert.spv";
    std::string fragPath = baseDir + logicalName + ".frag.spv";

    rhi::IShader* vs = nullptr;
    rhi::IShader* fs = nullptr;
    if (!TryLoadShaderPairVK(device, vertPath.c_str(), fragPath.c_str(), vs, fs) || !vs || !fs) {
        if (vs) DestroyShaderVK(device, vs);
        if (fs) DestroyShaderVK(device, fs);
        return nullptr;
    }
    PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.debugName = shaderName;
    desc.vertexLayout = layout;
    desc.topology = topology;
    desc.layoutKind = layoutKind;
    if (depthStencilOverride)
        desc.depthStencil = *depthStencilOverride;
    else {
        desc.depthStencil.depthTest = true;
        desc.depthStencil.depthWrite = true;
    }
    IPipeline* pipe = CreateGraphicsPipelineVK(device, desc);
    DestroyShaderVK(device, vs);
    DestroyShaderVK(device, fs);
    return pipe;
}

IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader) {
    return CreateComputePipelineVK(device, computeShader, PipelineLayoutKind::Legacy);
}

IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader, PipelineLayoutKind layoutKind) {
    if (!device) return nullptr;
    auto* p = new PipelineVK(device);
    p->m_isCompute = true;
    p->m_layoutKind = layoutKind;
    p->m_requiredSetCount = static_cast<uint8_t>(vkres::GetRequiredDescriptorSetCount(layoutKind));
    void* layoutToUse = nullptr;
    if (layoutKind != PipelineLayoutKind::Legacy) {
        layoutToUse = device->GetPipelineLayoutForKind(layoutKind);
    }
    if (!layoutToUse) {
        layoutToUse = device->GetComputePipelineLayout();
    }
    if (!computeShader || !layoutToUse || !device->GetPipelineCache()) return p;
    ShaderVK* shader = static_cast<ShaderVK*>(computeShader);
    if (!shader->m_module) return p;
    p->m_pipeline = CreateVkComputePipeline(device, layoutToUse, device->GetPipelineCache(), shader->m_module);
    p->m_nativePipelineLayout = (layoutKind != PipelineLayoutKind::Legacy) ? layoutToUse : nullptr;
    return p;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
