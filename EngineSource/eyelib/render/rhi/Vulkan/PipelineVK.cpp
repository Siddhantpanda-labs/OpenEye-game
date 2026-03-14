#include "eyelib/render/rhi/Vulkan/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

PipelineVK::PipelineVK(DeviceVK* device) : m_device(device) {}

PipelineVK::~PipelineVK() = default;

void PipelineVK::GetComputeBlockPresence(ComputeBlockPresence& out) const { out = m_computeBlockPresence; }

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

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

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

} // namespace rhi
} // namespace render
} // namespace eyelib
