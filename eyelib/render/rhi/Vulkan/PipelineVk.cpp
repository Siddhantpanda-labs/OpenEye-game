#include "eyelib/render/rhi/Vulkan/PipelineVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include "eyelib/render/rhi/Vulkan/ShaderVk.h"
#include "eyelib/render/rhi/Vulkan/VkEnums.h"
#include <vulkan/vulkan_core.h>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

namespace {

constexpr uint32_t kMaxDescriptorBindings = 16;

void CreateDescriptorSetLayout(DeviceVk* device, VkDescriptorSetLayout* outLayout) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutBinding ubo = {};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(ubo);
    for (uint32_t i = 1; i <= 8; ++i) {
        VkDescriptorSetLayoutBinding sam = {};
        sam.binding = i;
        sam.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sam.descriptorCount = 1;
        sam.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        bindings.push_back(sam);
    }
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device->GetDevice(), &info, nullptr, outLayout);
}

} // namespace

PipelineVk* PipelineVk::CreateGraphics(DeviceVk* device, const PipelineDesc& desc) {
    if (!device || !device->GetDevice()) return nullptr;
    ShaderVk* vs = static_cast<ShaderVk*>(desc.vertexShader);
    ShaderVk* fs = static_cast<ShaderVk*>(desc.fragmentShader);
    if (!vs || !fs || !vs->IsValid() || !fs->IsValid()) return nullptr;
    auto* p = new PipelineVk();
    p->m_device = device;
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    CreateDescriptorSetLayout(device, &p->m_descriptorSetLayout);
    if (p->m_descriptorSetLayout == VK_NULL_HANDLE) { delete p; return nullptr; }
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = 256;
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &p->m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device->GetDevice(), &layoutInfo, nullptr, &p->m_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device->GetDevice(), p->m_descriptorSetLayout, nullptr);
        delete p;
        return nullptr;
    }
    VkShaderStageFlagBits vsStage = vk::VkEnums::ShaderStage(ShaderStage::Vertex);
    VkShaderStageFlagBits fsStage = vk::VkEnums::ShaderStage(ShaderStage::Fragment);
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = vsStage;
    stages[0].module = vs->m_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = fsStage;
    stages[1].module = fs->m_module;
    stages[1].pName = "main";
    std::vector<VkVertexInputAttributeDescription> attrs;
    std::vector<VkVertexInputBindingDescription> bindings;
    if (!desc.vertexLayout.attributes.empty()) {
        VkVertexInputBindingDescription binding = {};
        binding.binding = 0;
        binding.stride = desc.vertexLayout.strideInBytes;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(binding);
        for (const auto& a : desc.vertexLayout.attributes) {
            VkVertexInputAttributeDescription attr = {};
            attr.location = a.location;
            attr.binding = 0;
            attr.format = vk::VkEnums::VertexFormat(a.type);
            attr.offset = a.offsetInBytes;
            attrs.push_back(attr);
        }
    }
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vi.pVertexBindingDescriptions = bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = vk::VkEnums::PrimitiveTopology(desc.topology);
    ia.primitiveRestartEnable = VK_FALSE;
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = desc.raster.cullEnable ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    rs.frontFace = vk::VkEnums::FrontFace(desc.raster.frontCounterClockwise);
    rs.depthBiasEnable = desc.raster.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = desc.raster.depthBiasConstant;
    rs.depthBiasSlopeFactor = desc.raster.depthBiasSlope;
    rs.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms.sampleShadingEnable = VK_FALSE;
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = desc.depthStencil.depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = desc.depthStencil.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = vk::VkEnums::DepthCompare(desc.depthStencil.depthCompareLess);
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = desc.depthStencil.stencilTest ? VK_TRUE : VK_FALSE;
    VkPipelineColorBlendAttachmentState blendAtt = {};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAtt.blendEnable = desc.blend.blendEnable ? VK_TRUE : VK_FALSE;
    blendAtt.srcColorBlendFactor = vk::VkEnums::BlendFactor(desc.blend.srcColor);
    blendAtt.dstColorBlendFactor = vk::VkEnums::BlendFactor(desc.blend.dstColor);
    blendAtt.colorBlendOp = vk::VkEnums::BlendOp(desc.blend.colorOp);
    blendAtt.srcAlphaBlendFactor = vk::VkEnums::BlendFactor(desc.blend.srcAlpha);
    blendAtt.dstAlphaBlendFactor = vk::VkEnums::BlendFactor(desc.blend.dstAlpha);
    blendAtt.alphaBlendOp = vk::VkEnums::BlendOp(desc.blend.alphaOp);
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.logicOpEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = &blendAtt;
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn = {};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo gp = {};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = p->m_layout;
    gp.renderPass = device->GetDefaultRenderPass();
    gp.subpass = 0;
    gp.basePipelineHandle = VK_NULL_HANDLE;
    gp.basePipelineIndex = -1;
    if (vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &gp, nullptr, &p->m_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device->GetDevice(), p->m_layout, nullptr);
        vkDestroyDescriptorSetLayout(device->GetDevice(), p->m_descriptorSetLayout, nullptr);
        delete p;
        return nullptr;
    }
    return p;
}

PipelineVk* PipelineVk::CreateCompute(DeviceVk* device, ShaderVk* computeShader) {
    if (!device || !computeShader || !computeShader->IsValid()) return nullptr;
    auto* p = new PipelineVk();
    p->m_device = device;
    CreateDescriptorSetLayout(device, &p->m_descriptorSetLayout);
    if (p->m_descriptorSetLayout == VK_NULL_HANDLE) { delete p; return nullptr; }
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &p->m_descriptorSetLayout;
    if (vkCreatePipelineLayout(device->GetDevice(), &layoutInfo, nullptr, &p->m_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device->GetDevice(), p->m_descriptorSetLayout, nullptr);
        delete p;
        return nullptr;
    }
    VkPipelineShaderStageCreateInfo stage = {};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = computeShader->m_module;
    stage.pName = "main";
    VkComputePipelineCreateInfo cp = {};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp.stage = stage;
    cp.layout = p->m_layout;
    if (vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &cp, nullptr, &p->m_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device->GetDevice(), p->m_layout, nullptr);
        vkDestroyDescriptorSetLayout(device->GetDevice(), p->m_descriptorSetLayout, nullptr);
        delete p;
        return nullptr;
    }
    return p;
}

void PipelineVk::Destroy(DeviceVk* device, PipelineVk* pipeline) {
    if (!device || !pipeline) return;
    if (pipeline->m_pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device->GetDevice(), pipeline->m_pipeline, nullptr);
    if (pipeline->m_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device->GetDevice(), pipeline->m_layout, nullptr);
    if (pipeline->m_descriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device->GetDevice(), pipeline->m_descriptorSetLayout, nullptr);
    delete pipeline;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
